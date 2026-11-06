"""Build only our six-owned-record ESL; no source mods or game files required."""
from pathlib import Path
import json,struct,sys
def render(spec):
 def emit(nodes):
  out=bytearray()
  for node in nodes:
   h=bytearray.fromhex(node["header"])
   if "group" in node:
    body=emit(node["children"]);struct.pack_into("<I",h,4,len(body)+24)
   else:
    body=b"".join(q["tag"].encode()+struct.pack("<H",len(bytes.fromhex(q["hex"])))+bytes.fromhex(q["hex"]) for q in node["fields"])
    struct.pack_into("<I",h,4,len(body))
   out+=h+body
  return bytes(out)
 return emit(spec["nodes"])
if __name__=="__main__":
 root=Path(__file__).resolve().parents[1];dest=Path(sys.argv[1])if len(sys.argv)>1 else root/"build/PackWeaponCondition.esp"
 dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(render(json.loads((root/"source/plugin.json").read_text())))
 print("Built",dest)
