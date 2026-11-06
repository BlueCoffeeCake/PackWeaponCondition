from pathlib import Path
import json,re,hashlib,xml.etree.ElementTree as ET
from build_plugin import render
root=Path(__file__).resolve().parents[1];compat=json.loads((root/"compatibility.json").read_text())
catalog=(root/"native/WeaponCatalog.h").read_text()
keys=[n.lower()+"|"+f"{int(v,16):06x}"for n,v in re.findall(r'\{"([^"]+)",0x([0-9a-fA-F]+),',catalog)]
assert len(keys)==len(set(keys))==compat["managedWeaponRecords"]==703
wp=json.loads((root/"config/F4SE/Plugins/ConditionSystemFramework/Durability/Consumption/Weapons/VanillaWeapons.json").read_text())
norm=lambda k:k.split("|")[0].lower()+"|"+f"{int(k.split('|')[1],16):06x}"
profile=[norm(k)for q in wp["profiles"]for k in q["formIDs"]];assert len(profile)==len(set(profile))==len(keys)and set(profile)==set(keys)
assert not any("dcguard_overhaul.esp"in k for k in keys)
ammo=json.loads((root/"config/F4SE/Plugins/ConditionSystemFramework/Durability/Consumption/Ammos/PackAmmos.json").read_text());a=[norm(k)for q in ammo["profiles"]for k in q["formIDs"]];assert len(a)==len(set(a))==compat["ammoIdentifiers"]==290
spec=json.loads((root/"source/plugin.json").read_text());b=render(spec);assert hashlib.sha256(b).hexdigest()==spec["sourcePluginSHA256"]==compat["sourcePluginSHA256"]
records=[r for n in spec["nodes"]for r in(n.get("children",[])if"group"in n else[n])];assert len(records)==7
assert sum(r["record"]=="GLOB"for r in records)==5 and sum(r["record"]=="PERK"for r in records)==1
assert "GNU GENERAL PUBLIC LICENSE"in(root/"LICENSE").read_text()
assert(root/"licenses/CSF-COPYRIGHT.txt").is_file()and(root/"third_party/minhook/LICENSE.txt").is_file()
for q in json.loads((root/"native-source-provenance.json").read_text()):assert hashlib.sha256((root/q["file"]).read_bytes()).hexdigest()==q["sha256"]
ET.parse(root/"signing/artifact-configuration.xml")
for p in root.rglob("*"):
 if not p.is_file()or any(t in(".git","build","out","dist","__pycache__")for t in p.relative_to(root).parts):continue
 assert p.suffix.lower()not in(".dll",".exe",".pdb",".lib",".o",".obj",".ba2",".esp",".esm",".esl",".pfx",".pem",".key"),p
 content=p.read_bytes().lower();markers=(b"c:"+bytes([92])+b"users"+bytes([92]),b"c:"+b"/users/",b"d:"+b"/steam",b"refresh_"+b"token=",b"client_"+b"secret=");assert not any(t in content for t in markers),p
for name in("source-checks.yml","signing.yml"):
 text=(root/".github/workflows"/name).read_text();assert not re.search(r"uses:\s*[^\s]+@(?![0-9a-f]{40}(?:\s|$))",text)
sign=(root/".github/workflows/signing.yml").read_text();assert "workflow_dispatch:"in sign and "SIGNPATH_APPROVED == 'true'"in sign and "github.event.repository.private == false"in sign
assert "runs-on: windows-2022"in sign and "retention-days: 1"in sign
print("PASS: GPL source boundaries, 703 weapon profiles, 290 ammo IDs, six owned-record plugin reconstruction, source hashes, XML and gated/pinned workflows. No native code executed.")
