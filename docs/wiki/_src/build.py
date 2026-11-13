#!/usr/bin/env python3
"""Generates the static wiki (docs/wiki/**/*.html) from content_concepts.py + content_controls.py.
Run:  python3 docs/wiki/_src/build.py      (no dependencies). Edit the content files, not the generated HTML."""
import os, html, sys
sys.path.insert(0, os.path.dirname(__file__))
from content_concepts import CONCEPTS
from content_controls import CONTROLS, CONTROL_GROUPS
from content_designer import DESIGNER

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def esc(s): return html.escape(s, quote=False)

def box(kind, title, text): return f'<div class="box {kind}"><b class="h">{title}</b>{text}</div>'

def build_nav(rel, cur):
    def a(path, title):
        c = ' cur' if path == cur else ''
        return f'<a class="item{c}" href="{rel}{path}">{title}</a>'
    out = [f'<a class="brand" href="{rel}index.html">SuperGUI wiki</a>',
           '<input id="navfilter" placeholder="Filter pages..." autocomplete="off">']
    out.append('<h4>Concepts</h4>')
    for c in CONCEPTS: out.append(a(f'concepts/{c["slug"]}.html', c['title']))
    out.append('<h4>Form Designer</h4>')
    for c in DESIGNER: out.append(a(f'designer/{c["slug"]}.html', c['title']))
    for g, slugs in CONTROL_GROUPS:
        out.append(f'<h4>{g}</h4>')
        for s in slugs:
            ctl = next(x for x in CONTROLS if x['slug'] == s)
            out.append(a(f'controls/{s}.html', ctl['cls']))
    return '\n'.join(out)

def page(path, title, body, prevnext=None):
    rel = '../' * path.count('/')
    nav = build_nav(rel, path)
    pn = ''
    if prevnext:
        p, n = prevnext
        pn = '<div class="pn"><span>' + (f'&larr; <a href="{rel}{p[0]}">{p[1]}</a>' if p else '') + '</span><span>' + (f'<a href="{rel}{n[0]}">{n[1]}</a> &rarr;' if n else '') + '</span></div>'
    doc = f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{esc(title)} - SuperGUI wiki</title><link rel="stylesheet" href="{rel}assets/wiki.css"></head>
<body><nav class="side">
{nav}
</nav><main>
{body}
{pn}
</main><script src="{rel}assets/wiki.js"></script></body></html>
'''
    full = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    open(full, 'w').write(doc)

def control_body(c):
    kinds = {'edit': 'Editable (modal edit)', 'instant': 'Instant (acts on click)', 'display': 'Display only', 'user': 'Your drawing'}
    b = [f'<h1>{c["cls"]}</h1>', f'<p class="tag">{c["tag"]}</p>',
         f'<div class="badges"><span>{kinds[c["kind"]]}</span><span>src/{c["header"]}</span>' +
         ''.join(f'<span>{esc(x)}</span>' for x in c.get('badges', [])) + '</div>']
    imgs = []
    for suffix, cap in (('', 'Focused, idle'), ('_edit', 'While editing')):
        f = f'img/{c["slug"]}{suffix}.png'
        if os.path.exists(os.path.join(ROOT, f)):
            if c['kind'] in ('display', 'user', 'instant') or (suffix == '' and not os.path.exists(os.path.join(ROOT, f'img/{c["slug"]}_edit.png'))):
                cap = c.get('shotcap', 'Rendered by the library')
            imgs.append(f'<figure><img src="../{f}" alt="{c["cls"]} {cap}" style="image-rendering:pixelated;max-width:100%"><figcaption>{cap}</figcaption></figure>')
    if c.get('extra_img'):
        f, cap = c['extra_img']
        imgs.append(f'<figure><img src="../{f}" alt="{cap}" style="image-rendering:pixelated;max-width:100%"><figcaption>{cap}</figcaption></figure>')
    if imgs:
        b.append('<h2>What it looks like</h2><div style="display:flex;flex-wrap:wrap;gap:20px">' + ''.join(imgs) + '</div><p class="tag" style="font-size:13px">Real renders from the library (headless, dark theme, Font2, shown at 2x).</p>')
    b.append('<h2>Quick start</h2>' + f'<pre><code>{esc(c["code"])}</code></pre>')
    b.append('<h2>Unique features</h2><ul>' + ''.join(f'<li>{x}</li>' for x in c['features']) + '</ul>')
    b.append('<h2>GUI workflow</h2>' + c['gui'])
    b.append('<h2>Event workflow</h2>' + c['events'])
    if c.get('api'):
        b.append('<h2>Main API</h2><table><tr><th>Call</th><th>What it does</th></tr>' +
                 ''.join(f'<tr><td><code>{esc(k)}</code></td><td>{v}</td></tr>' for k, v in c['api']) + '</table>')
    if c.get('tips'): b.append('<h2>Tips and tricks</h2>' + ''.join(box('tip', 'Tip', t) for t in c['tips']))
    if c.get('pitfalls'): b.append('<h2>Pitfalls</h2>' + ''.join(box('warn', 'Watch out', t) for t in c['pitfalls']))
    if c.get('see'):
        b.append('<h2>See also</h2><p>' + ' &middot; '.join(c['see']) + '</p>')
    return '\n'.join(b)

def main():
    flat = [(f'concepts/{c["slug"]}.html', c['title']) for c in CONCEPTS]
    dflat = [(f'designer/{c["slug"]}.html', c['title']) for c in DESIGNER]
    flat += dflat
    for g, slugs in CONTROL_GROUPS:
        for s in slugs:
            ctl = next(x for x in CONTROLS if x['slug'] == s)
            flat.append((f'controls/{s}.html', ctl['cls']))
    for i, c in enumerate(CONCEPTS):
        page(f'concepts/{c["slug"]}.html', c['title'], f'<h1>{c["title"]}</h1>' + c['body'], (flat[i-1] if i else None, flat[i+1]))
    for j, c in enumerate(DESIGNER):
        i = len(CONCEPTS) + j
        page(f'designer/{c["slug"]}.html', c['title'], f'<h1>{c["title"]}</h1>' + c['body'], (flat[i-1], flat[i+1]))
    n = len(CONCEPTS) + len(DESIGNER)
    for j, (path, title) in enumerate(flat[n:]):
        s = path.split('/')[-1][:-5]
        ctl = next(x for x in CONTROLS if x['slug'] == s)
        page(path, ctl['cls'], control_body(ctl), (flat[n+j-1], flat[n+j+1] if n+j+1 < len(flat) else None))
    cards = ''.join(f'<a href="controls/{s}.html">{next(x for x in CONTROLS if x["slug"]==s)["cls"]}<small>{next(x for x in CONTROLS if x["slug"]==s)["short"]}</small></a>' for _, ss in CONTROL_GROUPS for s in ss)
    dcards = ''.join(f'<a href="designer/{c["slug"]}.html">{c["title"]}<small>{c["short"]}</small></a>' for c in DESIGNER)
    ccards = ''.join(f'<a href="concepts/{c["slug"]}.html">{c["title"]}<small>{c["short"]}</small></a>' for c in CONCEPTS)
    home = f'''<h1>SuperGUI controls wiki</h1>
<p class="tag">Encoder + two buttons + touch UI controls for ESP32 and RP2040, drawn with LovyanGFX sprites.</p>
<p>This wiki explains <b>each control</b> (what is special about it, how the user works it, which events it fires, tips and pitfalls) and the <b>concepts</b> they share. For the complete reference tables and RAM numbers see <code>README_Controls.md</code>; for a step-by-step tutorial see <code>GettingStarted_Controls.md</code>; for runnable sketches see <code>examples/</code>.</p>
<h2>Start here</h2><div class="grid">{ccards}</div>
<h2>Form Designer</h2><p>Lay out a screen with the mouse and get the C++ for it: <code>tools/FormDesigner</code>.</p><div class="grid">{dcards}</div>
<h2>Controls</h2><div class="grid">{cards}</div>
<h2>Using this wiki</h2><p>It is plain static HTML: open <code>docs/wiki/index.html</code> in a browser, no server needed. The pages are generated by <code>docs/wiki/_src/build.py</code> from the two content files next to it; edit those and rerun the script rather than editing the HTML.</p>'''
    page('index.html', 'Home', home)
    print('pages:', len(flat) + 1)
main()
