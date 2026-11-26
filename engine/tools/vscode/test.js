// Debugs the showcase from start to end through the extension, with a stand-in for VS Code, and fails if
// anything isn't as it should be: node tools/vscode/test.js path/to/nexgen [media folder]
'use strict';

const Module = require('module');
const path = require('path');
const fs = require('fs');

const program = path.resolve(process.argv[2] || '');
const media = path.resolve(process.argv[3] || path.join(__dirname, '..', '..', '..', 'NexgenRedux-Media', 'media'));

class Emitter {
    constructor() {
        this.listeners = [];
        this.event = f => this.listeners.push(f);
    }
    fire(x) {
        this.listeners.forEach(f => f(x));
    }
}
const realLoad = Module._load;
Module._load = function (name, ...rest) {
    const Uri = { file: p => ({ toString: () => 'file:///' + p.split(path.sep).join('/') }) };
    return name === 'vscode' ? { EventEmitter: Emitter, Uri, workspace: { workspaceFolders: [] } } : realLoad.call(this, name, ...rest);
};
const { Session, report } = require('./extension.js');

const session = new Session({ configuration: { program, media } });
let seq = 1;
const pending = new Map();
const events = [];
let look = null;
session.onDidSendMessage(m => {
    if (m.type === 'response') {
        pending.get(m.request_seq)(m);
        pending.delete(m.request_seq);
    } else if (m.event !== 'output') {
        events.push(m);
        if (look) {
            look();
        }
    }
});

function request(command, args) {
    return new Promise(resolve => {
        const r = { type: 'request', seq: seq++, command, arguments: args || {} };
        pending.set(r.seq, resolve);
        session.handleMessage(r);
    });
}

async function ok(command, args) {
    const m = await request(command, args);
    if (!m.success) {
        throw new Error(`${command}: ${m.message}`);
    }
    return m.body;
}

function waitFor(name) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error(`no ${name} event`)), 30000);
        look = () => {
            const i = events.findIndex(e => e.event === name);
            if (i >= 0) {
                clearTimeout(timer);
                look = null;
                resolve(events.splice(i, 1)[0].body);
            }
        };
        look();
    });
}

function check(what, good) {
    console.log(`${good ? 'ok  ' : 'FAIL'} ${what}`);
    if (!good) {
        throw new Error(what);
    }
}

(async () => {
    // The main screen's update: its first statement.
    const file = path.join(media, 'showcase', 'main', 'main.as');
    const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/);
    const line = lines.findIndex(l => /void update\(float/.test(l)) + 3;
    check(`a line to stop at in ${file}`, line > 3);

    await ok('initialize', { adapterID: 'nexgen' });
    await ok('launch', { program, args: ['--hidden'], port: 4719 });
    await waitFor('initialized');
    const set = await ok('setBreakpoints', { source: { path: file }, breakpoints: [{ line }] });
    check('breakpoint set', set.breakpoints[0].verified);
    await ok('configurationDone');

    check('stopped at the breakpoint', (await waitFor('stopped')).reason === 'breakpoint');
    const frames = (await ok('stackTrace', { threadId: 1 })).stackFrames;
    check(`in ${frames[0].name}, line ${frames[0].line}`, frames[0].line === line && /update/.test(frames[0].name));
    check('the file found again', frames[0].source.path && path.resolve(frames[0].source.path) === path.resolve(file));

    const scopes = (await ok('scopes', { frameId: frames[0].id })).scopes;
    const locals = (await ok('variables', { variablesReference: scopes[0].variablesReference })).variables;
    const dt = locals.find(v => v.name === 'dt');
    check(`dt = ${dt && dt.value}`, dt && dt.type === 'float' && parseFloat(dt.value) > 0);
    const self = locals.find(v => v.name === 'this');
    check('this can be opened', self && self.variablesReference > 0);
    const members = (await ok('variables', { variablesReference: self.variablesReference })).variables;
    const widget = members.find(v => v.type === 'Widget');
    check('a member', members.length > 0 && widget);
    const inside = (await ok('variables', { variablesReference: widget.variablesReference })).variables;
    check(`the engine's ${widget.name} through its properties (${inside.length})`, inside.some(v => v.name === 'visible' && v.value === 'true'));
    const hover = await ok('evaluate', { expression: `${widget.name}.visible`, frameId: frames[0].id, context: 'hover' });
    check(`${widget.name}.visible is ${hover.result}`, hover.result === 'true');
    check('an unknown name is refused', !(await request('evaluate', { expression: 'nothing_here', frameId: frames[0].id, context: 'hover' })).success);

    await ok('next', { threadId: 1 });
    check('stepped', (await waitFor('stopped')).reason === 'step');
    const after = (await ok('stackTrace', { threadId: 1 })).stackFrames[0];
    check(`to line ${after.line}`, after.line !== line || after.name !== frames[0].name);

    await ok('setBreakpoints', { source: { path: file }, breakpoints: [] });
    await ok('continue', { threadId: 1 });
    await ok('pause', { threadId: 1 });
    check('paused', (await waitFor('stopped')).reason === 'pause');
    await ok('stepOut', { threadId: 1 });
    check('stepped out', (await waitFor('stopped')).reason === 'step');
    await ok('continue', { threadId: 1 });

    // Where the time goes: samples of what runs, while it runs.
    await ok('nexgenProfile', { on: true });
    await new Promise(resolve => setTimeout(resolve, 1500));
    const profile = await ok('nexgenProfile', { on: false });
    const names = (profile.functions || []).map(f => f.name);
    check(`profiled: ${profile.samples} samples in ${(profile.seconds || 0).toFixed(1)} s`, profile.samples > 20 && profile.seconds > 1);
    check(`the screen's own functions among them (${names.slice(0, 3).join(', ')})`, names.some(n => /MainScreen::(update|draw)/.test(n)));
    check('lines too, found in the media', (profile.lines || []).length > 0 && profile.lines.some(l => l.local));
    check('a report of it', /Nexgen Redux script profile/.test(report(profile)) && /MainScreen/.test(report(profile)));

    await ok('disconnect', { terminateDebuggee: true });
    console.log('all of it works');
    process.exit(0);
})().catch(error => {
    console.log(`FAIL ${error.message}`);
    try {
        session.child && session.child.kill();
    } catch (e) {
    }
    process.exit(1);
});
