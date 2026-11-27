// Debugs Nexgen Redux scripts from VS Code. VS Code speaks the Debug Adapter Protocol to this file, and this file
// speaks to the engine's script debugger (engine/script/debugger.h) over TCP: JSON, a line each. Nothing to install
// or build: it is plain JavaScript, using only what VS Code and Node.js already have.
'use strict';

const vscode = require('vscode');
const net = require('net');
const path = require('path');
const fs = require('fs');
const childProcess = require('child_process');

const THREAD = 1;

class Session {
    constructor(session) {
        this.session = session;
        this.config = session.configuration;
        this.sendEmitter = new vscode.EventEmitter();
        this.onDidSendMessage = this.sendEmitter.event;
        this.seq = 1;
        this.socket = null;
        this.buffer = '';
        this.asked = 0;
        this.waiting = new Map();     // what the engine is yet to answer, by id
        this.handles = new Map();     // variablesReference -> what to ask the engine for
        this.nextHandle = 1;
        this.child = null;
        this.ended = false;
        this.roots = this.mediaRoots();
    }

    // --- Files: the engine names them from its media folder, as "showcase/blocks/main.as" ---

    mediaRoots() {
        const roots = [];
        const folders = (vscode.workspace.workspaceFolders || []).map(f => f.uri.fsPath);
        const add = p => { if (p && fs.existsSync(p) && !roots.includes(p)) roots.push(path.resolve(p)); };
        const given = this.config.media;
        if (given) {
            add(path.isAbsolute(given) ? given : path.join(folders[0] || '', given));
        }
        for (const f of folders) {
            add(path.join(f, 'media'));
            add(path.join(f, 'NexgenRedux-Media', 'media'));
            add(f);
        }
        if (this.config.program) {
            add(path.join(path.dirname(this.config.program), 'media'));
        }
        return roots;
    }

    toEngine(file) {
        const full = path.resolve(file);
        const same = (a, b) => process.platform === 'win32' ? a.toLowerCase() === b.toLowerCase() : a === b;
        for (const root of this.roots) {
            const prefix = root.endsWith(path.sep) ? root : root + path.sep;
            if (full.length > prefix.length && same(full.slice(0, prefix.length), prefix)) {
                return full.slice(prefix.length).split(path.sep).join('/');
            }
        }
        // Somewhere else: from a folder called media, if it is in one.
        const parts = full.split(path.sep);
        const at = parts.map(p => p.toLowerCase()).lastIndexOf('media');
        return at >= 0 ? parts.slice(at + 1).join('/') : parts.slice(-2).join('/');
    }

    toLocal(file) {
        for (const root of this.roots) {
            const there = path.join(root, ...file.split('/'));
            if (fs.existsSync(there)) {
                return there;
            }
        }
        return null;
    }

    // --- To VS Code ---

    send(message) {
        message.seq = this.seq++;
        this.sendEmitter.fire(message);
    }

    event(name, body) {
        this.send({ type: 'event', event: name, body: body || {} });
    }

    reply(request, body, error) {
        this.send({ type: 'response', request_seq: request.seq, command: request.command, success: !error, message: error, body: body || {} });
    }

    log(text, category) {
        this.event('output', { category: category || 'console', output: text.endsWith('\n') ? text : text + '\n' });
    }

    // --- To the engine ---

    tell(cmd, fields) {
        if (this.socket) {
            this.socket.write(JSON.stringify(Object.assign({ cmd }, fields || {})) + '\n');
        }
    }

    ask(cmd, fields) {
        return new Promise(resolve => {
            if (!this.socket) {
                resolve({});
                return;
            }
            const id = ++this.asked;
            this.waiting.set(id, resolve);
            this.tell(cmd, Object.assign({ id }, fields || {}));
        });
    }

    heard(line) {
        let m;
        try {
            m = JSON.parse(line);
        } catch (e) {
            return;
        }
        if (m.id && this.waiting.has(m.id)) {
            const resolve = this.waiting.get(m.id);
            this.waiting.delete(m.id);
            resolve(m);
            return;
        }
        if (m.event === 'stopped') {
            this.handles.clear();
            const body = { reason: m.reason, threadId: THREAD, allThreadsStopped: true };
            if (m.text) {
                body.text = m.text;
                body.description = 'Error: ' + m.text;
            }
            this.event('stopped', body);
        } else if (m.event === 'output') {
            this.log(m.text, m.level === 'error' ? 'stderr' : 'stdout');
        }
    }

    connect(host, port, seconds) {
        const until = Date.now() + seconds * 1000;
        return new Promise((resolve, reject) => {
            const tryOnce = () => {
                const socket = net.createConnection({ host, port });
                socket.setNoDelay(true);
                socket.once('connect', () => {
                    socket.removeAllListeners('error');
                    resolve(socket);
                });
                socket.once('error', error => {
                    socket.destroy();
                    if (Date.now() < until && !this.ended) {
                        setTimeout(tryOnce, 250);
                    } else {
                        reject(error);
                    }
                });
            };
            tryOnce();
        });
    }

    async open(host, port, seconds) {
        this.socket = await this.connect(host, port, seconds);
        this.socket.setEncoding('utf8');
        this.socket.on('data', data => {
            this.buffer += data;
            let end;
            while ((end = this.buffer.indexOf('\n')) >= 0) {
                const line = this.buffer.slice(0, end);
                this.buffer = this.buffer.slice(end + 1);
                this.heard(line);
            }
        });
        this.socket.on('close', () => this.finish());
        this.socket.on('error', () => {});
        const hello = await this.ask('hello');
        this.log(`Connected to Nexgen Redux on ${host}:${port}, AngelScript ${hello.angelscript || '?'}. Media from ${this.roots[0] || '(none found: set "media")'}`);
    }

    finish() {
        if (this.ended) {
            return;
        }
        this.ended = true;
        for (const resolve of this.waiting.values()) {
            resolve({});
        }
        this.waiting.clear();
        this.event('terminated');
    }

    handle(kind) {
        const id = this.nextHandle++;
        this.handles.set(id, kind);
        return id;
    }

    describe(v) {
        return { name: v.name, value: v.value, type: v.type, variablesReference: v.ref ? this.handle({ ref: v.ref }) : 0 };
    }

    // --- From VS Code ---

    handleMessage(request) {
        if (request.type !== 'request') {
            return;
        }
        this.run(request).catch(error => this.reply(request, {}, String(error && error.message || error)));
    }

    async run(r) {
        const a = r.arguments || {};
        switch (r.command) {
        case 'initialize':
            this.reply(r, {
                supportsConfigurationDoneRequest: true,
                supportsEvaluateForHovers: true,
                supportsTerminateRequest: true,
            });
            break;

        case 'launch': {
            const port = a.port || 4711;
            const program = a.program;
            if (!program || !fs.existsSync(program)) {
                this.reply(r, {}, `The engine isn't at "${program}": set "program" in launch.json.`);
                return;
            }
            const args = (a.args || []).concat(['--debug', String(port), '--debug-wait']);
            this.child = childProcess.spawn(program, args, { cwd: a.cwd || path.dirname(program), stdio: ['ignore', 'ignore', 'pipe'] });
            this.child.stderr.on('data', d => this.log(String(d), 'stderr'));
            this.child.on('exit', code => {
                this.event('exited', { exitCode: code || 0 });
                this.finish();
            });
            this.child.on('error', e => this.log(String(e), 'stderr'));
            await this.open('127.0.0.1', port, 20);
            this.reply(r);
            this.event('initialized');
            break;
        }

        case 'attach':
            await this.open(a.host || '127.0.0.1', a.port || 4711, a.timeout || 10);
            this.reply(r);
            this.event('initialized');
            break;

        case 'setBreakpoints': {
            const file = this.toEngine(a.source.path);
            const lines = (a.breakpoints || []).map(b => b.line);
            await this.ask('breakpoints', { file, lines });
            this.reply(r, { breakpoints: lines.map(line => ({ verified: true, line })) });
            break;
        }

        case 'setExceptionBreakpoints':
            this.reply(r, { breakpoints: [] });
            break;

        case 'configurationDone':
            await this.ask('ready');
            this.reply(r);
            break;

        case 'threads':
            this.reply(r, { threads: [{ id: THREAD, name: 'Scripts' }] });
            break;

        case 'stackTrace': {
            const answer = await this.ask('stack');
            const frames = (answer.frames || []).map(f => {
                const local = this.toLocal(f.file);
                const source = local ? { name: path.basename(local), path: local } : { name: f.file, presentationHint: 'deemphasize' };
                return { id: f.frame + 1, name: f.name, source, line: f.line, column: f.column || 1 };
            });
            this.reply(r, { stackFrames: frames, totalFrames: frames.length });
            break;
        }

        case 'scopes': {
            const frame = a.frameId - 1;
            this.reply(r, { scopes: [
                { name: 'Locals', presentationHint: 'locals', variablesReference: this.handle({ frame, scope: 'locals' }), expensive: false },
                { name: 'Globals', variablesReference: this.handle({ frame, scope: 'globals' }), expensive: false },
            ] });
            break;
        }

        case 'variables': {
            const kind = this.handles.get(a.variablesReference);
            const answer = kind ? await this.ask('variables', kind) : {};
            this.reply(r, { variables: (answer.variables || []).map(v => this.describe(v)) });
            break;
        }

        case 'evaluate': {
            const frame = a.frameId ? a.frameId - 1 : 0;
            const answer = await this.ask('evaluate', { frame, expr: a.expression });
            if (answer.result) {
                const v = this.describe(answer.result);
                this.reply(r, { result: v.value, type: v.type, variablesReference: v.variablesReference });
            } else {
                this.reply(r, {}, answer.error || 'not known here');
            }
            break;
        }

        case 'continue':
            this.tell('continue');
            this.reply(r, { allThreadsContinued: true });
            break;
        case 'next':
        case 'stepIn':
        case 'stepOut':
        case 'pause':
            this.tell(r.command);
            this.reply(r);
            break;

        case 'nexgenProfile': {
            const answer = await this.ask('profile', { on: !!a.on });
            if (!a.on) {
                for (const f of (answer.functions || []).concat(answer.lines || [])) {
                    f.local = this.toLocal(f.file);
                }
            }
            this.reply(r, answer);
            break;
        }

        // The entities of the screen on top, one's properties, and changing one.
        case 'nexgenEntities':
            this.reply(r, await this.ask('entities', {}));
            break;
        case 'nexgenEntity':
            this.reply(r, await this.ask('entity', { entity: a.entity }));
            break;
        case 'nexgenSetEntity':
            this.reply(r, await this.ask('setEntity', { entity: a.entity, property: a.property, value: String(a.value) }));
            break;

        case 'terminate':
        case 'disconnect':
            this.ended = true;
            if (this.socket) {
                this.socket.destroy();
                this.socket = null;
            }
            const stop = r.command === 'terminate' || (a.terminateDebuggee !== false && this.child);
            if (stop && this.child && this.child.exitCode === null) {
                this.child.kill();
            }
            this.reply(r);
            if (r.command === 'terminate') {
                this.event('terminated');
            }
            break;

        default:
            this.reply(r, {}, `${r.command} isn't supported`);
        }
    }

    dispose() {
        this.ended = true;
        if (this.socket) {
            this.socket.destroy();
        }
    }
}

// A profile as a page to read: the functions that took the most, by themselves and with what they call, and the busiest lines.
function report(p) {
    const samples = p.samples || 0;
    const percent = n => samples ? (100 * n / samples).toFixed(1) + '%' : '-';
    const where = f => f.local ? `[${path.basename(f.local)}:${f.line}](${vscode.Uri.file(f.local).toString()}#L${f.line})` : `${f.file}:${f.line}`;
    const out = [];
    out.push('# Nexgen Redux script profile', '');
    out.push(`${(p.seconds || 0).toFixed(1)} seconds, ${samples} samples taken about a millisecond apart while scripts ran. A function's *total* is`);
    out.push('the time spent in it and in what it called; *itself* is the time in its own lines.', '');
    out.push('| Function | Total | Itself | Where |', '|---|---:|---:|---|');
    for (const f of p.functions || []) {
        out.push(`| \`${f.name.replace(/\|/g, '\\|')}\` | ${percent(f.total)} | ${percent(f.self)} | ${where(f)} |`);
    }
    out.push('', '## The busiest lines', '', '| Line | Time |', '|---|---:|');
    for (const l of p.lines || []) {
        out.push(`| ${where(l)} | ${percent(l.self)} |`);
    }
    if (!samples) {
        out.push('', 'No script ran while it was profiling.');
    }
    return out.join('\n');
}

function activate(context) {
    const session = () => {
        const s = vscode.debug.activeDebugSession;
        if (!s || s.type !== 'nexgen') {
            vscode.window.showWarningMessage('Start debugging Nexgen Redux first.');
            return null;
        }
        return s;
    };
    context.subscriptions.push(vscode.commands.registerCommand('nexgen.profileStart', async () => {
        const s = session();
        if (s) {
            await s.customRequest('nexgenProfile', { on: true });
            vscode.window.setStatusBarMessage('$(pulse) Profiling Nexgen Redux scripts...', 4000);
        }
    }));
    context.subscriptions.push(vscode.commands.registerCommand('nexgen.profileStop', async () => {
        const s = session();
        if (s) {
            const p = await s.customRequest('nexgenProfile', { on: false });
            const document = await vscode.workspace.openTextDocument({ content: report(p), language: 'markdown' });
            await vscode.commands.executeCommand('markdown.showPreview', document.uri).then(undefined, () => vscode.window.showTextDocument(document));
        }
    }));
    // The entities of the screen on top, as a tree beside the call stack: each with its properties and its children,
    // looked at again every second and a half while the game runs.
    const entities = new Entities(session);
    context.subscriptions.push(vscode.window.registerTreeDataProvider('nexgenEntities', entities));
    context.subscriptions.push(vscode.commands.registerCommand('nexgen.refreshEntities', () => entities.refresh()));
    context.subscriptions.push(vscode.commands.registerCommand('nexgen.changeProperty', async item => {
        const s = session();
        if (!s || !item || !item.property) {
            return;
        }
        const value = await vscode.window.showInputBox({ prompt: `${item.property.name} of ${item.owner}`, value: item.property.value });
        if (value !== undefined) {
            const answer = await s.customRequest('nexgenSetEntity', { entity: item.entity, property: item.property.name, value });
            if (!answer.done) {
                vscode.window.showWarningMessage(`${item.property.name} can't be ${value}.`);
            }
            entities.refresh();
        }
    }));
    const timer = setInterval(() => {
        const s = vscode.debug.activeDebugSession;
        if (s && s.type === 'nexgen') {
            entities.refresh();
        }
    }, 1500);
    context.subscriptions.push({ dispose: () => clearInterval(timer) });
    context.subscriptions.push(vscode.debug.onDidTerminateDebugSession(() => entities.refresh()));

    context.subscriptions.push(vscode.debug.registerDebugAdapterDescriptorFactory('nexgen', {
        createDebugAdapterDescriptor: session => new vscode.DebugAdapterInlineImplementation(new Session(session)),
    }));
    // F5 with no launch.json: attach to an engine started with --debug.
    context.subscriptions.push(vscode.debug.registerDebugConfigurationProvider('nexgen', {
        resolveDebugConfiguration(folder, config) {
            if (!config.type && !config.request && !config.name) {
                return { type: 'nexgen', request: 'attach', name: 'Attach to Nexgen Redux', port: 4711 };
            }
            return config;
        },
    }));
}

class Entities {
    constructor(session) {
        this.session = session;
        this.changed = new vscode.EventEmitter();
        this.onDidChangeTreeData = this.changed.event;
        this.list = [];
    }

    refresh() {
        this.changed.fire();
    }

    async getChildren(item) {
        const s = vscode.debug.activeDebugSession;
        if (!s || s.type !== 'nexgen') {
            return [];
        }
        try {
            if (!item) {
                const answer = await s.customRequest('nexgenEntities', {});
                this.list = answer.entities || [];
                return this.list.filter(e => !e.parent).map(e => this.entityItem(e));
            }
            if (item.entity && !item.property) {
                const answer = await s.customRequest('nexgenEntity', { entity: item.entity });
                const owner = item.label;
                const properties = (answer.properties || []).map(p => {
                    const t = new vscode.TreeItem(p.name);
                    t.description = p.value;
                    t.tooltip = `${p.name}: ${p.value}`;
                    t.entity = item.entity;
                    t.owner = owner;
                    t.property = p;
                    t.contextValue = p.changes ? 'nexgenChangeable' : 'nexgenProperty';
                    return t;
                });
                const children = this.list.filter(e => e.parent === item.entity).map(e => this.entityItem(e));
                return properties.concat(children);
            }
        } catch (e) {
            return [];
        }
        return [];
    }

    entityItem(e) {
        const t = new vscode.TreeItem(e.name || `#${e.entity & 0xFFFF}`, vscode.TreeItemCollapsibleState.Collapsed);
        t.id = String(e.entity);
        t.description = `${e.space ? 'space' : 'plane'} ${e.place}`;
        t.entity = e.entity;
        t.contextValue = 'nexgenEntity';
        t.iconPath = new vscode.ThemeIcon(e.space ? 'symbol-namespace' : 'symbol-field');
        return t;
    }

    getTreeItem(item) {
        return item;
    }
}

function deactivate() {}

module.exports = { activate, deactivate, Session, report };
