"""Builds dist/NPC_AI_AutoPatch_<version>.zip: a FOMOD installer with the new DLL and the older (legacy) DLL, the profiles, the languages, README and LICENSE.

    python tools/make_release.py <version> <new DLL> <older DLL>

The new DLL is the alandtse NG build (ng-build/build/release/out/NPCAIAutoPatch), the older one comes from legacy/build.cmd. Nothing is uploaded.
"""
import hashlib
import json
import os
import shutil
import sys
import xml.dom.minidom
import zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version, dll_new, dll_old = sys.argv[1], sys.argv[2], sys.argv[3]
for d in (dll_new, dll_old):
    assert os.path.isfile(d), f"missing DLL: {d}"
NAME = "NPC AI AutoPatch"
NEW_VER = "1.6.1170.0"
out_dir = os.path.join(root, "dist")
stage = os.path.join(out_dir, "_stage")
if os.path.exists(stage):
    shutil.rmtree(stage)
os.makedirs(os.path.join(stage, "fomod"))


def read(p):
    return open(os.path.join(root, p), encoding="utf-8").read()


def write(p, text):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w", encoding="utf-8", newline="\n").write(text)


plugins = os.path.join(stage, "common", "SKSE", "Plugins")
for sub in ("profiles", "lang"):
    src = os.path.join(root, sub)
    for n in sorted(os.listdir(src)):
        if n.endswith(".json"):
            json.load(open(os.path.join(src, n), encoding="utf-8"))  # must parse
            os.makedirs(os.path.join(plugins, "NPCAIAutoPatch", sub), exist_ok=True)
            shutil.copyfile(os.path.join(src, n), os.path.join(plugins, "NPCAIAutoPatch", sub, n))
shutil.copyfile(os.path.join(root, "LICENSE"), os.path.join(plugins, "NPCAIAutoPatch-LICENSE.txt"))
for key, d in (("new", dll_new), ("older", dll_old)):
    t = os.path.join(stage, key, "SKSE", "Plugins")
    os.makedirs(t)
    shutil.copyfile(d, os.path.join(t, "NPCAIAutoPatch.dll"))

write(os.path.join(stage, "README.txt"),
      "NPC AI AutoPatch installer: it asks which NPCAIAutoPatch.dll (new: Skyrim 1.6.1170 and newer; older: Skyrim VR or 1.6.1130 and older; both work on 1.6.1170).\n\n" + read("README.md"))

config = f"""<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">
  <moduleName>{NAME}</moduleName>
  <requiredInstallFiles>
    <folder source="common" destination=""/>
    <file source="README.txt" destination="{NAME} README.txt"/>
  </requiredInstallFiles>
  <installSteps order="Explicit">
    <installStep name="Game version">
      <optionalFileGroups order="Explicit">
        <group name="Which NPCAIAutoPatch.dll do you want?" type="SelectExactlyOne">
          <plugins order="Explicit">
            <plugin name="Skyrim 1.6.1170 and newer (SE / AE)">
              <description>The new build. Works on Skyrim SE/AE 1.6.1170 and on every later version, including 1.7.x. Pre-selected when the installer sees a game version of 1.6.1170 or newer.</description>
              <conditionFlags><flag name="dll">new</flag></conditionFlags>
              <typeDescriptor><dependencyType><defaultType name="Optional"/><patterns>
                <pattern><dependencies><gameDependency version="{NEW_VER}"/></dependencies><type name="Recommended"/></pattern>
              </patterns></dependencyType></typeDescriptor>
            </plugin>
            <plugin name="Skyrim VR, or 1.6.1130 and older">
              <description>The older build, for Skyrim VR and for SE/AE 1.6.1130 and older. Pre-selected when the installer sees an older game version or Skyrim VR. On 1.6.1170 either build works.</description>
              <conditionFlags><flag name="dll">older</flag></conditionFlags>
              <typeDescriptor><dependencyType><defaultType name="Recommended"/><patterns>
                <pattern><dependencies><gameDependency version="{NEW_VER}"/></dependencies><type name="Optional"/></pattern>
              </patterns></dependencyType></typeDescriptor>
            </plugin>
          </plugins>
        </group>
      </optionalFileGroups>
    </installStep>
  </installSteps>
  <conditionalFileInstalls>
    <patterns>
      <pattern>
        <dependencies><flagDependency flag="dll" value="new"/></dependencies>
        <files><folder source="new" destination=""/></files>
      </pattern>
      <pattern>
        <dependencies><flagDependency flag="dll" value="older"/></dependencies>
        <files><folder source="older" destination=""/></files>
      </pattern>
    </patterns>
  </conditionalFileInstalls>
</config>
"""
info = (f"<fomod>\n  <Name>{NAME}</Name>\n  <Author>CageTV</Author>\n  <Version>{version}</Version>\n"
        "  <Website>https://github.com/CageTV/NPC_AI_AutoPatch</Website>\n"
        "  <Description>Puts AI Overhaul's AI back on the NPCs a later mod overrides and gives mod-added town people the AI of Immersive Citizens - AI Overhaul, at game start, with no patch plugin.</Description>\n"
        "</fomod>\n")
xml.dom.minidom.parseString(config)
xml.dom.minidom.parseString(info)
write(os.path.join(stage, "fomod", "ModuleConfig.xml"), config)
write(os.path.join(stage, "fomod", "info.xml"), info)

zpath = os.path.join(out_dir, f"NPC_AI_AutoPatch_{version}.zip")
if os.path.exists(zpath):
    os.remove(zpath)
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for dp, _, fs in os.walk(stage):
        for fn in sorted(fs):
            p = os.path.join(dp, fn)
            z.write(p, os.path.relpath(p, stage).replace("\\", "/"))
shutil.rmtree(stage)

with zipfile.ZipFile(zpath) as z:
    for i in z.infolist():
        print(f"{i.file_size:>10}  {i.filename}")
print(zpath)
print("sha256", hashlib.sha256(open(zpath, "rb").read()).hexdigest())
