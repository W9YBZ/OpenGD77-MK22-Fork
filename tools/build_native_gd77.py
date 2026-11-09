#!/usr/bin/env python3
"""Build the original MCUXpresso GD-77 preset and produce a checked CPS ZIP."""
import argparse,json,os,re,stat,subprocess
from pathlib import Path
import xml.etree.ElementTree as X
from package_native_gd77 import package,ROOT

def audit_project(before,after):
    def rows(data):
        root=X.fromstring(data)
        cfg=next(c for c in root.findall('.//cconfiguration') if c.find('storageModule').get('name')=='GD-77')
        return [(e.tag,dict(e.attrib),(e.text or '').strip()) for e in cfg.iter()]
    old,new=rows(before),rows(after);assert len(old)==len(new),'native preset shape changed'
    changes=[]
    for a,b in zip(old,new):
        if a==b:continue
        attrs=dict(b[1]);value=attrs.pop('value',None)
        assert a[0]==b[0]=='option' and a[2]==b[2]
        assert attrs==a[1] and value=='Default' and attrs.get('name')=='Heap and Stack placement','unexpected native preset change'
        changes.append(dict(option='Heap and Stack placement',added_value='Default',effect='manual firmware.ld remains selected'))
    return dict(active_preset='GD-77',changes=changes,other_options_unchanged=True)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,default=ROOT)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--name',default='OpenGD77-TalkPermit')
    p.add_argument('--ide-root',type=Path,default=Path.home()/'.local/opt/nxp/mcuxpressoide-24.12.148')
    p.add_argument('--headless-deps',type=Path,default=Path.home()/'.local/opt/nxp/headless-deps')
    p.add_argument('--donor-zip',type=Path)
    a=p.parse_args();source=a.source.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    ide=a.ide_root.resolve();version=re.search(r'mcuxpressoide-(\d+)\.(\d+)\.(\d+)',ide.name,re.I)
    assert version and tuple(map(int,version.groups()[:2]))<=(24,12),'requires MCUXpresso IDE <=24.12'
    executable=ide/'ide/mcuxpressoide';assert executable.is_file(),'IDE not installed'
    tools=next(ide.glob('ide/plugins/com.nxp.mcuxpresso.tools.linux_*/tools'))
    compiler=tools/'bin/arm-none-eabi-gcc';assert (tools/'arm-none-eabi/lib/thumb/v7e-m+fp/hard/libcr_c.a').exists()
    fw=source/'firmware'
    subprocess.run(['git','-C',str(source),'diff','--exit-code','HEAD','--','firmware'],check=True)
    project=X.parse(fw/'.cproject');cfg=next(c for c in project.findall('.//cconfiguration') if c.find('storageModule').get('name')=='GD-77')
    active=cfg.find('.//configuration').get('postbuildStep','').split('#')[0]
    assert 'FirmwareLoader' not in active and 'codec_cleaner' in active
    env=os.environ.copy();env['PATH']=str(a.headless_deps/'usr/bin')+os.pathsep+env['PATH']
    env['LD_LIBRARY_PATH']=str(a.headless_deps/'usr/lib/x86_64-linux-gnu')+os.pathsep+env.get('LD_LIBRARY_PATH','')
    xvfb=a.headless_deps/'usr/bin/xvfb-run';assert xvfb.is_file(),'install Xvfb for Eclipse UI-dependent build plugins'
    cmd=[str(xvfb),'-a',str(executable),'-nosplash','--launcher.suppressErrors','-application','org.eclipse.cdt.managedbuilder.core.headlessbuild','-data',str(out/'workspace'),'-no-indexer','-import',str(fw),'-cleanBuild','firmware/GD-77','-printErrorMarkers']
    metadata={p:p.read_bytes() for p in [fw/'.project',fw/'.cproject']}
    helpers=[fw/'tools/codec_cleaner',fw/'tools/codec_cleaner.Linux'];modes={p:stat.S_IMODE(p.stat().st_mode) for p in helpers}
    (out/'build-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
    (out/'compiler.log').write_text(subprocess.check_output([str(compiler),'--version'],text=True))
    # Never allow a failed build to retain a stale final ZIP/manifest.
    for name in [a.name+'.zip',a.name+'.zip.unchecked','manifest.json']:(out/name).unlink(missing_ok=True)
    (fw/'GD-77/OpenGD77.bin').unlink(missing_ok=True)
    try:
        for p,mode in modes.items():p.chmod(mode|stat.S_IXUSR)
        with (out/'codec.log').open('w') as log:subprocess.run([str(helpers[1]),'-C'],cwd=fw/'linkerdata',stdout=log,stderr=subprocess.STDOUT,check=True)
        with (out/'build.log').open('w') as log:r=subprocess.run(cmd,env=env,cwd=source,stdout=log,stderr=subprocess.STDOUT)
        assert r.returncode==0,'native build failed; see build.log'
        log=(out/'build.log').read_text()
        assert 'Headless build completed successfully' in log and '-D__REDLIB__' in log and '-flto' not in log
        assert '-T ../linkerscripts/firmware.ld' in log
        compiled=(fw/'.cproject').read_bytes()
        audit=audit_project(metadata[fw/'.cproject'],compiled)
        (out/'project-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
        (out/'compiled-project.cproject').write_bytes(compiled)
    finally:
        for p,data in metadata.items():p.write_bytes(data)
        for p,mode in modes.items():p.chmod(mode)
    package(source,out,a.name,a.donor_zip)

if __name__=='__main__':main()
