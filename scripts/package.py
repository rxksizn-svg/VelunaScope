from pathlib import Path
import zipfile, hashlib, json
root=Path.cwd()
bundle=root/'build/Scope_artefacts/Release/VST3/Veluna Scope.vst3'
if not bundle.is_dir() or not any(p.is_file() for p in bundle.rglob('*.vst3')):
    raise RuntimeError('Scope VST3 bundle missing')
out=root/'dist'
out.mkdir(exist_ok=True)
manifest={}
with zipfile.ZipFile(out/'VelunaScope-Windows-x64.zip','w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(bundle.rglob('*')):
        if p.is_file():
            name=str(Path('VST3')/bundle.name/p.relative_to(bundle)).replace('\\','/')
            z.write(p,name);manifest[name]=hashlib.sha256(p.read_bytes()).hexdigest()
    for name in ('คู่มือภาษาไทย.md','README.md','SOURCES.md'):
        z.write(root/name,'Docs/'+name)
    for relative in ('build/_deps/ebur-src/COPYING','build/_deps/vad-src/LICENSE','build/_deps/vad-src/PATENTS'):
        p=root/relative
        if not p.is_file():raise RuntimeError('Dependency license file missing: '+relative)
        z.write(p,'Licenses/'+p.parent.name+'/'+p.name)
    z.writestr('SHA256.json',json.dumps(manifest,indent=2))
print(out/'VelunaScope-Windows-x64.zip')
