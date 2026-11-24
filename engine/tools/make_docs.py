"""Makes the documentation web site from the guide, the generated script reference and the demos.

    python tools/make_docs.py <folder to write>

Needs only Python. The pages need no server and load nothing from elsewhere.
"""
import html
import json
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, '..', '..', 'docs')
SHOWCASE = os.path.join(HERE, '..', 'media', 'showcase')
GITHUB = 'https://github.com/Team-Resurgent/NexgenRedux'

CSS = '''
:root { --back: #0f1219; --panel: #171c28; --line: #283044; --text: #dfe5f0; --dim: #93a0b8; --accent: #5b9bff; --code: #11151f; --kw: #7fb2ff; --type: #5fd6c0; --str: #f0b878; --num: #d6a6ff; --com: #6f7d96; }
@media (prefers-color-scheme: light) { :root { --back: #fafbfd; --panel: #ffffff; --line: #d9dfea; --text: #1c2433; --dim: #5c687d; --accent: #1b62d6; --code: #f1f4f9; --kw: #1b4fb8; --type: #0b7a68; --str: #9a5a0c; --num: #7a35b8; --com: #77839a; } }
* { box-sizing: border-box; }
html { scroll-padding-top: 72px; }
body { margin: 0; background: var(--back); color: var(--text); font: 16px/1.6 system-ui, "Segoe UI", Helvetica, Arial, sans-serif; }
a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }
header { position: sticky; top: 0; z-index: 2; display: flex; flex-wrap: wrap; gap: 4px 22px; align-items: center; padding: 10px 24px; background: var(--panel); border-bottom: 1px solid var(--line); }
header .name { font-weight: 700; font-size: 20px; color: var(--text); margin-right: 10px; }
header a.here { color: var(--text); border-bottom: 2px solid var(--accent); }
header input { margin-left: auto; min-width: 200px; flex: 0 1 300px; padding: 7px 12px; border: 1px solid var(--line); border-radius: 8px; background: var(--back); color: var(--text); font: inherit; font-size: 15px; }
.page { display: grid; grid-template-columns: 230px minmax(0, 1fr); max-width: 1240px; margin: 0 auto; }
.page.wide { grid-template-columns: minmax(0, 1fr); }
nav { position: sticky; top: 58px; align-self: start; max-height: calc(100vh - 58px); overflow: auto; padding: 24px 12px 40px 24px; font-size: 15px; }
nav b { display: block; margin: 16px 0 2px; font-size: 13px; text-transform: uppercase; letter-spacing: .06em; }
nav b:first-child { margin-top: 0; }
nav i, .kind { font: 13px ui-monospace, Consolas, Menlo, monospace; color: var(--dim); opacity: .8; }
.kind { border: 1px solid var(--line); border-radius: 6px; padding: 2px 7px; vertical-align: middle; margin-left: 6px; }
nav a { display: block; padding: 2px 0; color: var(--dim); overflow-wrap: anywhere; }
main { padding: 24px 24px 80px; min-width: 0; }
h1 { font-size: 34px; line-height: 1.2; margin: 8px 0 16px; }
h2 { font-size: 24px; margin: 40px 0 8px; padding-top: 12px; border-top: 1px solid var(--line); }
h3 { font-size: 18px; margin: 24px 0 4px; }
.scroll { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; margin: 12px 0; }
td, th { text-align: left; vertical-align: top; padding: 7px 12px 7px 0; border-bottom: 1px solid var(--line); }
.api td:first-child { width: 52%; }
code { font: 14px/1.5 ui-monospace, Consolas, Menlo, monospace; }
p code, li code, td code { background: var(--code); border-radius: 5px; padding: 1px 5px; }
.api td code { background: none; padding: 0; }
pre { background: var(--code); border: 1px solid var(--line); border-radius: 10px; padding: 14px 16px; overflow-x: auto; }
.k { color: var(--kw); } .t { color: var(--type); } .s { color: var(--str); } .n { color: var(--num); } .c { color: var(--com); font-style: italic; }
.tag { font-size: 13px; color: var(--dim); }
.shots { display: grid; grid-template-columns: repeat(auto-fill, minmax(270px, 1fr)); gap: 14px; margin: 20px 0; }
.shots img { width: 100%; height: auto; border-radius: 10px; border: 1px solid var(--line); display: block; }
.shots figure { margin: 0; } .shots figcaption { color: var(--dim); font-size: 14px; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(230px, 1fr)); gap: 14px; margin: 24px 0; }
.cards a { display: block; padding: 16px 18px; background: var(--panel); border: 1px solid var(--line); border-radius: 12px; color: var(--dim); }
.cards b { display: block; color: var(--text); font-size: 18px; }
.lead { font-size: 19px; color: var(--dim); max-width: 46em; }
details { border: 1px solid var(--line); border-radius: 10px; margin: 10px 0; background: var(--panel); }
summary { cursor: pointer; padding: 10px 16px; } details pre { border-width: 1px 0 0; border-radius: 0 0 10px 10px; margin: 0; }
.none { color: var(--dim); }
[hidden] { display: none !important; }
@media (max-width: 800px) { .page { grid-template-columns: minmax(0, 1fr); } nav { display: none; } main { padding: 16px 16px 60px; } header { padding: 10px 16px; } .api td { display: block; width: auto !important; border-bottom: 0; padding: 2px 0; } .api tr { display: block; padding: 8px 0; border-bottom: 1px solid var(--line); } }
'''

SEARCH = '''
const box = document.getElementById('search');
const none = document.getElementById('none');
function filter() {
  const words = box.value.toLowerCase().split(/\\s+/).filter(Boolean);
  let any = false;
  for (const part of document.querySelectorAll('section')) {
    const title = part.dataset.title || '';
    const whole = words.every(w => title.includes(w));
    let shown = whole;
    for (const item of part.querySelectorAll('[data-find]')) {
      const hit = whole || words.every(w => (title + ' ' + item.dataset.find).includes(w));
      item.hidden = !hit;
      shown = shown || hit;
    }
    part.hidden = !shown;
    any = any || shown;
  }
  none.hidden = any;
}
box.addEventListener('input', filter);
document.addEventListener('keydown', e => {
  if (e.key === 'F4' || (e.key === '/' && document.activeElement !== box)) { e.preventDefault(); box.focus(); box.select(); }
});
'''

KEYWORDS = set('class void const if else for while do return break continue switch case default int uint float bool string array in out null true false this is cast funcdef auto and or not property get set public private'.split())


def slug(text):
    return re.sub(r'[^a-z0-9]+', '-', text.lower()).strip('-')


def highlight(code):
    out = []
    pattern = r'(//[^\n]*|/\*.*?\*/)|("(?:\\.|[^"\\\n])*")|(\b\d+\.?\d*f?\b)|([A-Za-z_]\w*)|(.)'
    for comment, string, number, word, other in re.findall(pattern, code, re.S):
        if comment:
            out.append('<span class="c">%s</span>' % html.escape(comment))
        elif string:
            out.append('<span class="s">%s</span>' % html.escape(string))
        elif number:
            out.append('<span class="n">%s</span>' % number)
        elif word:
            kind = 'k' if word in KEYWORDS else 't' if word[0].isupper() else ''
            out.append('<span class="%s">%s</span>' % (kind, word) if kind else word)
        else:
            out.append(html.escape(other))
    return ''.join(out)


def inline(text):
    parts = re.split(r'(`[^`]*`)', text)
    for i, part in enumerate(parts):
        if part.startswith('`') and part.endswith('`') and len(part) > 1:
            parts[i] = '<code>%s</code>' % html.escape(part[1:-1])
        else:
            part = html.escape(part)
            part = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', part)
            parts[i] = re.sub(r'\*\*([^*]+)\*\*', r'<b>\1</b>', part)
    return ''.join(parts)


def cells(line):
    # A bar inside code isn't a cell's edge.
    return [c.strip() for c in re.split(r'\|(?=(?:[^`]*`[^`]*`)*[^`]*$)', line.strip().strip('|'))]


def markdown(text):
    """As much of Markdown as the guide uses."""
    out, lines, i, heads = [], text.split('\n'), 0, []
    while i < len(lines):
        line = lines[i]
        if line.startswith('```'):
            i += 1
            code = []
            while not lines[i].startswith('```'):
                code.append(lines[i])
                i += 1
            out.append('<pre><code>%s</code></pre>' % highlight('\n'.join(code)))
        elif line.startswith('#'):
            level = len(line) - len(line.lstrip('#'))
            title = line.lstrip('# ')
            if level == 2:
                heads.append(title)
            out.append('<h%d id="%s">%s</h%d>' % (level, slug(title), inline(title), level))
        elif line.startswith('|'):
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                rows.append(cells(lines[i]))
                i += 1
            i -= 1
            out.append('<div class="scroll"><table><tr>%s</tr>' % ''.join('<th>%s</th>' % inline(c) for c in rows[0]))
            out += ['<tr>%s</tr>' % ''.join('<td>%s</td>' % inline(c) for c in row) for row in rows[2:]]
            out.append('</table></div>')
        elif line.startswith('- '):
            items = []
            while i < len(lines) and (lines[i].startswith('- ') or lines[i].startswith('  ')):
                if lines[i].startswith('- '):
                    items.append(lines[i][2:])
                else:
                    items[-1] += ' ' + lines[i].strip()
                i += 1
            i -= 1
            out.append('<ul>%s</ul>' % ''.join('<li>%s</li>' % inline(t) for t in items))
        elif line.strip():
            words = []
            while i < len(lines) and lines[i].strip() and not re.match(r'[#|`-]', lines[i]):
                words.append(lines[i].strip())
                i += 1
            if words:
                i -= 1
                out.append('<p>%s</p>' % inline(' '.join(words)))
            else:
                out.append('<p>%s</p>' % inline(line))
        i += 1
    return '\n'.join(out), heads


def tidy(declaration):
    """How a property is written by someone using it."""
    m = re.match(r'(.+?) get_(\w+)\(\) const property$', declaration)
    return '%s %s' % m.groups() if m else declaration


# How the reference is arranged. A name that isn't here is put at the end.
TOPICS = [
    ('Screens', ['Screen', 'screens', 'app']),
    ('Drawing', ['Canvas', 'Color', 'Texture', 'Font', 'Blend', 'Filter', 'Align']),
    ('Assets', ['assets']),
    ('Sound', ['Sound', 'Voice', 'Bus', 'audio']),
    ('Input', ['input']),
    ('Movement', ['Tween', 'Ease', 'Coroutines', 'Maths']),
    ('Saving', ['save']),
]
FUNCTIONS = ['Coroutines', 'Maths']  # sections that aren't a type or a namespace


def reference():
    sections, current = {}, None
    for line in open(os.path.join(DOCS, 'ScriptReference.md'), encoding='utf-8').read().split('\n'):
        if line.startswith('## ') or line.startswith('### '):
            title = line.lstrip('# ')
            # A function type is described with the calls that take it.
            kind = re.match(r'void (\w+)\(\)$', title)
            name = kind.group(1) + 's' if kind else title
            current = sections.setdefault(name, {'text': [], 'rows': []})
            function_type = 'funcdef ' + title if kind else None
        elif current is not None and line.startswith('| `'):
            declaration, doc = cells(line)[:2]
            current['rows'].append((tidy(declaration.strip('`')), doc))
        elif current is not None and line.strip() and not line.startswith('|'):
            if function_type:
                current['rows'].append((function_type, line.strip()))
            else:
                current['text'].append(line)

    known = [name for _, names in TOPICS for name in names]
    topics = TOPICS + [('More', [name for name in sections if name not in known])]
    out, nav = [], []
    for topic, names in topics:
        names = [name for name in names if name in sections]
        if not names:
            continue
        nav.append((None, topic))
        for name in names:
            section = sections[name]
            if name in FUNCTIONS:
                kind = 'functions'
            elif name[0].islower():
                kind = name + '::'
            elif all('(' not in d and ' ' not in d for d, _ in section['rows']):
                kind = 'enum'
            else:
                kind = 'type'
            # Every title is capitalised; what follows it says how it is written in a script.
            shown = name[0].upper() + name[1:]
            anchor = 'ns-' + slug(name) if kind.endswith('::') else slug(name)
            nav.append((anchor, shown + (' <i>%s</i>' % kind if kind.endswith('::') else '')))
            out.append('<section id="%s" data-title="%s"><h2>%s <span class="kind">%s</span></h2>' % (anchor, html.escape((topic + ' ' + name + ' ' + kind).lower(), True), shown, kind))
            out += ['<p>%s</p>' % inline(t) for t in section['text']]
            if section['rows']:
                out.append('<table class="api">')
                for declaration, doc in section['rows']:
                    out.append('<tr data-find="%s"><td><code>%s</code></td><td>%s</td></tr>' % (html.escape((declaration + ' ' + doc).lower(), True), highlight(declaration), inline(doc)))
                out.append('</table>')
            out.append('</section>')
    return '\n'.join(out), nav


def demos():
    found = []
    for name in os.listdir(SHOWCASE):
        info = os.path.join(SHOWCASE, name, 'screen.json')
        if os.path.exists(info):
            found.append((json.load(open(info, encoding='utf-8')), name))
    found.sort(key=lambda f: f[0].get('order', 0))
    out, nav, category = [], [], None
    groups = {}
    for info, name in found:
        groups.setdefault(info.get('category') or 'Main menu', []).append((info, name))
    for category, items in groups.items():
        out.append('<section id="%s" data-title="%s"><h2>%s</h2>' % (slug(category), category.lower(), category))
        nav.append((slug(category), category))
        for info, name in items:
            code = open(os.path.join(SHOWCASE, name, 'main.as'), encoding='utf-8').read()
            shot = os.path.join(DOCS, 'screenshots', name + '.png')
            find = html.escape((info['title'] + ' ' + info.get('description', '') + ' ' + name).lower(), True)
            out.append('<div data-find="%s"><h3 id="demo-%s">%s</h3><p>%s</p>' % (find, name, inline(info['title']), inline(info.get('description', ''))))
            if os.path.exists(shot):
                out.append('<div class="shots"><img loading="lazy" src="screenshots/%s.png" alt="%s"></div>' % (name, html.escape(info['title'])))
            out.append('<details><summary><code>%s/main.as</code> <span class="tag">%d lines</span></summary><pre><code>%s</code></pre></details></div>' % (name, code.count('\n'), highlight(code)))
        out.append('</section>')
    return '\n'.join(out), nav, len(found) - 1


def page(folder, name, title, body, nav=None, search=False):
    links = [('index.html', 'Home'), ('guide.html', 'Guide'), ('reference.html', 'Script reference'), ('demos.html', 'Demos')]
    top = ' '.join('<a%s href="%s">%s</a>' % (' class="here"' if f == name else '', f, t) for f, t in links)
    side = '<nav>%s</nav>' % ''.join('<a href="#%s">%s</a>' % (a, t) if a else '<b>%s</b>' % t for a, t in nav) if nav else ''
    text = '''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%s</title>
<style>%s</style>
</head>
<body>
<header><a class="name" href="index.html">Nexgen Redux</a> %s <a href="%s">GitHub</a> %s</header>
<div class="page%s">%s<main>%s%s</main></div>
%s</body>
</html>
''' % (html.escape(title), CSS, top, GITHUB, '<input id="search" type="search" placeholder="Search (F4)" aria-label="Search">' if search else '', '' if nav else ' wide', side, body,
       '<p class="none" id="none" hidden>Nothing found.</p>' if search else '', '<script>%s</script>\n' % SEARCH if search else '')
    open(os.path.join(folder, name), 'w', encoding='utf-8', newline='\n').write(text)


def main(folder):
    os.makedirs(os.path.join(folder, 'screenshots'), exist_ok=True)
    shots = os.path.join(DOCS, 'screenshots')
    for name in os.listdir(shots):
        shutil.copyfile(os.path.join(shots, name), os.path.join(folder, 'screenshots', name))

    demo_body, demo_nav, count = demos()
    page(folder, 'demos.html', 'Nexgen Redux demos', '<h1>Demos</h1><p class="lead">Every screen of the showcase, with its whole script.</p>' + demo_body, demo_nav, True)
    body, nav = reference()
    page(folder, 'reference.html', 'Nexgen Redux script reference', '<h1>Script reference</h1><p class="lead">Everything that scripts can call, written by the engine itself. A tag like <code>assets::</code> is a namespace: its calls are written <code>assets::texture(...)</code>.</p>' + body, nav, True)
    body, heads = markdown(open(os.path.join(DOCS, 'Guide.md'), encoding='utf-8').read())
    page(folder, 'guide.html', 'Nexgen Redux guide', body, [(slug(h), html.escape(h)) for h in heads])

    captions = [('music', 'Music'), ('runes', 'Rune shift'), ('rally', 'Comet rally'), ('cracktro', 'Cracktro'), ('parallax', 'Parallax'), ('coroutines', 'Coroutines'), ('gauges', 'Gauges'), ('shapes', 'Shapes'), ('xbox-main', 'On the original Xbox, in xemu')]
    body = '''<h1>Nexgen Redux</h1>
<p class="lead">A cross-platform engine for games and dashboards, scripted in AngelScript. The same scripts run on the original Xbox, Windows, Linux and macOS.</p>
<div class="cards">
<a href="guide.html"><b>Guide</b>Your first screen, and how everything fits together</a>
<a href="reference.html"><b>Script reference</b>Every call, with search</a>
<a href="demos.html"><b>Demos</b>%d screens, with their scripts</a>
<a href="%s/actions/workflows/engine.yml"><b>Download</b>Builds for each platform</a>
<a href="v1/index.html"><b>First engine</b>The reference for the engine before this one</a>
</div>
<div class="shots">%s</div>''' % (count, GITHUB, ''.join('<figure><img loading="lazy" src="screenshots/%s.png" alt="%s"><figcaption>%s</figcaption></figure>' % (n, c, c) for n, c in captions))
    page(folder, 'index.html', 'Nexgen Redux', body)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
