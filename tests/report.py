"""Small static Markdown and HTML reports; no frontend dependencies."""
from collections import Counter, defaultdict
from html import escape
import json
from pathlib import Path


SCOPE = '''The protocol suite tests game-visible 8-bit exchanges, not a complete adapter emulator.
Original firmware runs from seeded RAM, without executing its reset/boot path.
It executes its serial parser, command dispatch, validation,
response framing, and UDP connection bookkeeping. EEPROM driver operations,
phone status/disconnect, and low-level socket calls are controlled substitutes.
Physical serial timing, the original TCP/IP stack, and a real network are not
exercised. Timer events are frozen: pending tests check that polling alone
does not complete a request, not its physical duration.
The separate AT diagnostics execute the original command-body parser and handlers
without firmware function substitutions. They inspect flags, settings, and cursor
progress at an internal boundary, not game-visible response bytes or the complete
phone transport. libmobile has no corresponding internal AT parser, so these
cases are not run against the library. The DDI candidate target flips one bit
in a private ROM copy at file offset 0xC3D0 (5EFF to 7EFF). Original-image results
remain separate. The 10,000-instruction ceiling detects nonreturning cases; it
is not a latency measurement. This repair is strongly supported, but the cause
of the defective byte and the integrity of the whole dump remain unproven.
The supplied Ghidra archive contains exactly the same bytes as these two dumps.
Passing both images corroborates these selected paths, not the entire dump.
Library-only runs use the recorded firmware contracts without requiring ROMs.
A FAIL is a contract difference, including intentional extensions and firmware
quirks; an ERROR is a harness/runtime problem. Neither is silently marked PASS.'''


def write_reports(directory, records, metadata):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    groups = defaultdict(Counter)
    for record in records:
        groups[record['target']][record['status']] += 1
    summary = ['| Target | PASS | FAIL | ERROR | SKIP |', '| --- | ---: | ---: | ---: | ---: |']
    for target, counts in groups.items():
        summary.append('| ' + target + ' | ' + ' | '.join(str(counts[s]) for s in ('PASS', 'FAIL', 'ERROR', 'SKIP')) + ' |')
    md = ['# Firmware conformance report', '', f'Generated: {metadata["generated"]}', '',
          f'Library source revision: `{metadata["library_revision"]}`.',
          f'Library source SHA-256: `{metadata["library_source_sha256"]}`.', '', *summary, '', '## Scope', '', SCOPE, '', '## Firmware inputs', '']
    for profile in metadata['profiles']:
        md.append(f'- `{profile["filename"]}`: `{profile["sha256"]}`')
    md += [f'- DDI AT candidate, in memory only: `{metadata["at_candidate"]["sha256"]}`', '',
           'See [the firmware audit](firmware_audit.md) for candidate classifications and limits.']
    md += ['', '## Contract differences', '']
    for record in records:
        if record['status'] not in ('FAIL', 'ERROR'):
            continue
        md += [f'### {record["target"]}: {record["name"]}', '', record['description'], '',
               '```text', record['detail'], '```', '']
    md += ['## All results', '', '| Target | Test | Result |', '| --- | --- | --- |']
    md += [f'| {r["target"]} | `{r["name"]}` | {r["status"]} |' for r in records]
    (directory / 'report.md').write_text('\n'.join(md) + '\n')
    (directory / 'results.json').write_text(json.dumps({'metadata': metadata, 'tests': records}, indent=2) + '\n')
    cards = ''.join(f'<article><h2>{escape(target)}</h2><p>' +
                    ' · '.join(f'<strong>{counts[s]}</strong> {s}' for s in ('PASS', 'FAIL', 'ERROR', 'SKIP')) +
                    '</p></article>' for target, counts in groups.items())
    rows = []
    for record in records:
        exchanges = '\n'.join(f'{e["command"]}: {e["payload"] or "(empty)"} → {e["reply"]}' +
                              (f'\n  {e["conditions"]}' if 'conditions' in e else '')
                              for e in record['exchanges'])
        detail = record['detail'] or 'Matches the test expectation.'
        environment = ', '.join(record['substitutions']) or ('Controlled libmobile host callbacks.' if 'libmobile' in record['target'] else 'None.')
        rows.append(f'<details data-status="{record["status"]}"><summary><span class="{record["status"]}">{record["status"]}</span>'
                    f'<span>{escape(record["name"])}</span><small>{escape(record["target"])}</small></summary>'
                    f'<p>{escape(record["description"])}</p><pre>{escape(detail)}</pre>'
                    f'<h3>Inputs and results</h3><pre>{escape(exchanges)}</pre>'
                    f'<p class="muted">Controlled firmware calls: {escape(environment)}</p></details>')
    html = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Mobile Adapter firmware conformance</title>
<style>
:root{font-family:system-ui,sans-serif;color:#e7edf4;background:#111820;color-scheme:dark}
body{max-width:1120px;margin:40px auto;padding:0 24px;line-height:1.6}h1{line-height:1.2}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px}
article,details,.scope{background:#1a2530;border:1px solid #344351;border-radius:10px;padding:16px}
h2{font-size:17px;margin:0}h3{font-size:14px}.scope{margin:24px 0}.muted,small{color:#afbecc}
.controls{display:flex;flex-wrap:wrap;gap:12px;margin:24px 0}input,select{font:inherit;padding:8px;border:1px solid #526476;border-radius:6px}
input{flex:1;min-width:200px}details{margin:8px 0}summary{cursor:pointer;display:flex;gap:16px;align-items:baseline}
summary span:nth-child(2){flex:1;overflow-wrap:anywhere}small{white-space:nowrap}
.PASS{color:#8cdfa4}.FAIL{color:#ffa79f}.ERROR{color:#ffce85}.SKIP{color:#b8c5d2}
pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:12px;background:#111820;padding:12px;border-radius:6px}
[hidden]{display:none}a{color:#a9d3ff}@media(max-width:700px){summary{flex-wrap:wrap}small{width:100%}}
</style><body><p class="muted">LIBMOBILE · FIRMWARE REFERENCE TESTS</p>
<h1>Mobile Adapter reference tests</h1><p>Game-protocol contracts and separate internal AT parser diagnostics.</p>
'''
    html += f'<div class="cards">{cards}</div><details class="scope"><summary>Scope, firmware inputs, and evidence</summary><p>{escape(SCOPE)}</p>'
    html += f'<p class="muted">Generated {escape(metadata["generated"])}<br>Library revision {metadata["library_revision"]}</p>'
    for profile in metadata['profiles']:
        html += f'<pre>{escape(profile["filename"])}\nSHA-256 {profile["sha256"]}</pre>'
    html += f'<pre>DDI AT candidate, in memory only\nSHA-256 {metadata["at_candidate"]["sha256"]}</pre>'
    html += '</details>'
    html += '<p><a href="report.md">Markdown report</a> · <a href="results.json">Machine-readable results</a> · <a href="firmware_audit.md">Firmware bit audit</a></p>'
    html += '<div class="controls"><input id="search" aria-label="Search tests" placeholder="Search tests or target">'
    html += '<select id="status" aria-label="Result filter"><option value="all">All results</option><option selected>FAIL</option><option>PASS</option><option>ERROR</option><option>SKIP</option></select></div>'
    html += '<p id="visible" aria-live="polite"></p>' + ''.join(rows)
    html += '''<script>
const search=document.querySelector('#search'),status=document.querySelector('#status');
function filter(){let visible=0;document.querySelectorAll('details[data-status]').forEach(row=>{
row.hidden=!(status.value==='all'||row.dataset.status===status.value)||!row.textContent.toLowerCase().includes(search.value.toLowerCase());
if(!row.hidden)visible++;});document.querySelector('#visible').textContent=visible+' tests shown';}
search.addEventListener('input',filter);status.addEventListener('change',filter);filter();
</script></body></html>'''
    (directory / 'report.html').write_text(html)
