"""Hibernation classification must not turn previews, cold boots or stale files into success."""
from pathlib import Path
import importlib.util
import json
import subprocess
import sys
import tempfile
r=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('hibernate_evidence',r/'tools/hibernation_evidence.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
resume='#[EB|H:IS] 1\n#[EB|H:RDST]\n#[EB|H:RDEND]\n#[EB|LOG:EXITBS:START] date\n'
for data in [resume.encode(),resume.encode('utf-16'),resume.encode('utf-16-be'),resume.encode()+b'\x00'*4096]:
 # Explicit UTF16-BE input needs a BOM; the ASCII-padding case must retain stages.
 if data==resume.encode('utf-16-be'):data=b'\xfe\xff'+data
 e=m.loader_evidence(data)
 assert e['furthest_loader_stage']=='image_read_complete_and_exitbs_reached'
 assert e['kernel_resume']=='unproven_by_bootloader_log' and e['display_resume']=='unproven_by_bootloader_log'
cold=b'#[EB.H.IS|!] Err(0xE) <- RT.GV boot-image-key GUID\n#[EB|H:IS] 0\n#[EB|H:NOT]\n#[EB|LOG:EXITBS:START] date\n'
e=m.loader_evidence(cold);assert e['furthest_loader_stage']=='no_resume_attempt_proven' and not e['loader_failure_marker']
assert e['missing_metadata_variables']==['boot-image-key']
for log,expected in [(b'#[EB|H:IS] 1','hibernate_metadata_detected'),(b'#[EB|H:RDST]','image_read_started'),(b'#[EB|H:RDEND]','image_read_complete'),(b'#[EB|H:FAIL] STOP 0x1C','loader_rejected_resume'),(b'', 'no_resume_attempt_proven')]:
 assert m.loader_evidence(log)['furthest_loader_stage']==expected
assert m.loader_evidence(resume.encode()+cold)['mixed_detection_markers']
assert not m.loader_evidence(b'#[EB|H:IS] 10')['hibernate_detected']
secret=b'IOHibernateRTCVariables=<SECRET-RTC-BYTES> boot-image-key=SECRET-KEY boot-signature=SECRET-SIGNATURE'
assert 'SECRET' not in json.dumps(m.loader_evidence(secret))
before={'boot_session_uuid':'A','gpu_exec':100,'memory_sentinel':{'nonce':'random-token','pid':123,'process_start':'start-A','counter':5}}
after={'boot_session_uuid':'A','gpu_exec':110,'memory_sentinel':dict(before['memory_sentinel'],counter=6,live_verified=True)}
e=m.session_evidence(before,after);assert e['live_memory_sentinel_continued'] and e['gpu_completion_counter_advanced_in_continued_session']
assert isinstance(e['genuine_s4'],str) and 'requires' in e['genuine_s4']
for field,value in [('nonce','other'),('pid',124),('pid',True),('process_start','new'),('counter',5),('counter',True),('live_verified',False),('live_verified',1)]:
 bad=dict(after,memory_sentinel=dict(after['memory_sentinel'],**{field:value}))
 assert not m.session_evidence(before,bad)['live_memory_sentinel_continued']
assert m.session_evidence({}, {})['session']=='unproven'
assert m.session_evidence(before,dict(after,boot_session_uuid='B',memory_sentinel={}))['session']=='new_boot_observed'
assert m.session_evidence(before,dict(after,boot_session_uuid='B'))['session']=='live_process_continuity_observed'
assert m.session_evidence(before,dict(after,gpu_exec=100))['gpu_completion_counter_advanced_in_continued_session'] is False
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'resume.txt').write_bytes(resume.encode());(p/'cold.txt').write_bytes(cold)
 result=subprocess.check_output([sys.executable,str(r/'tools/hibernation_evidence.py'),'loader',str(p/'resume.txt'),str(p/'cold.txt')],text=True)
 rows=json.loads(result);assert len(rows)==2 and rows[0]['hibernate_detected'] and not rows[1]['hibernate_detected']
print('PASS hibernation evidence: loader stages, cold/recovery boot distinction, padded/UTF16 logs, secret omission and live-process continuity without false S4 claims')
