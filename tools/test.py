#!/usr/bin/env python3
"""Run standalone CPU/mock tests; optional exact-donor replay."""
import argparse,concurrent.futures,json,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--donor',type=Path);p.add_argument('--accelerator',type=Path);p.add_argument('--reference-source',type=Path);p.add_argument('--reference-framebuffer',type=Path);a=p.parse_args()
if bool(a.reference_source)!=bool(a.reference_framebuffer):p.error('reference metadata replay requires both --reference-source and --reference-framebuffer')
O=R/'build/tests';O.mkdir(parents=True,exist_ok=True)
python_tests=['raptorlake_p_reference_redraw_adapter','raptorlake_p_reference_timing_ready','raptorlake_p_reference_scanout_gate','raptorlake_p_reference_probe_gate','raptorlake_p_reference_port_bindings','raptorlake_p_reference_port_observer','raptorlake_p_reference_early_adapter','raptorlake_p_reference_backlight_adapter','raptorlake_p_reference_barrier_adapter','raptorlake_p_reference_display_pipe_adapter','raptorlake_p_reference_port_adapter','raptorlake_p_reference_bridge_adapter','raptorlake_p_reference_abi_adapter','raptorlake_p_reference_modules_adapter','raptorlake_p_reference_panel_adapter','raptorlake_p_reference_worker_adapter','raptorlake_p_bringup_adapter','raptorlake_p_native_engines_adapter','raptorlake_p_caps_shadow_adapter','raptorlake_p_control_adapter','raptorlake_p_cache_transaction_adapter','raptorlake_p_subranges_adapter','raptorlake_p_cache_shadow_adapter','raptorlake_p_native_factory_adapter','raptorlake_p_page_array_adapter','raptorlake_p_page_pool_adapter','raptorlake_p_native_chain_adapter','raptorlake_p_native_pools_adapter','raptorlake_p_gtt_shadow_adapter','raptorlake_p_native_memory_adapter','raptorlake_p_native_adapter','raptorlake_p_rcs_adapter','raptorlake_p_bcs_adapter','raptorlake_p_probe','right_hotplug','xelp_mmio_bar','bigsur_pmap','tahoe_iosurface','tahoe_pmap','sequoia_pmap','sonoma_iosurface','sonoma_pmap','ventura_pmap','tgl_kernel_admission','monterey_pmap','xelp_forcewake_irq','boot_arguments','tgl_display_observers','tgl_stream_ack','tgl_transmitter_observer','tgl_transition_live','tgl_aux_live','tgl_bootstrap_host','tgl_clock_order','provider_binding','tgl_start_order','tgl_hdmi_wiring','tgl_bootstrap_aux','tgl_copy_routing','tgl_training_leases','tgl_wiring','tgl_boot_readout','tgl_pcode_gate','tgl_port_use','tgl_tc_cold','tgl_fia','tgl_port_map','tgl_probe','dvmt','host_publication']
python_tests += ['raptorlake_p_reference_nvram_adapter']
python_tests += ['xe_data_access']
python_tests += ['raptorlake_p_reference_tc_rescan']
python_tests += ['raptorlake_p_reference_tc_aux_bypass']
python_tests += ['raptorlake_p_reference_tc_agdc']
python_tests += ['raptorlake_p_reference_tc_live_adapter','raptorlake_p_reference_tc_live_installer']
jobs=[(p.stem,['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(R/'WhateverXe'),str(p),'-o',str(O/p.stem)],[str(O/p.stem)]) for p in sorted((R/'tests').glob('*.cpp')) if not p.stem.endswith('_fixture')]
jobs += [(n,[sys.executable,str(R/'tests'/(n+'.py'))],None) for n in python_tests]
if a.donor:jobs += [(n,[sys.executable,str(R/'tests'/(n+'.py')),str(a.donor.resolve())],None) for n in ['tgl_tc_native','tgl_tc_mode','tgl_mode_native']]
if a.accelerator:jobs += [('raptorlake_p_native_donor',[sys.executable,str(R/'tests/raptorlake_p_native_donor.py'),str(a.accelerator.resolve())],None)] + [('xelp_forcewake_irq_native',[sys.executable,str(R/'tests/xelp_forcewake_irq.py'),'--donor',str(a.accelerator.resolve())],None),('tgl_rpcs_native',[sys.executable,str(R/'tests/tgl_rpcs_native.py'),str(a.accelerator.resolve())],None)] + [(n,[sys.executable,str(R/'tests'/(n+'.py')),str(a.accelerator.resolve())],None) for n in ['raptorlake_p_scheduler_credit_donor','raptorlake_p_subranges_donor','raptorlake_p_cache_shadow_donor','raptorlake_p_native_factory_donor','raptorlake_p_manager_pools_donor','raptorlake_p_page_array_donor','raptorlake_p_page_pool_donor','raptorlake_p_native_chain_donor','raptorlake_p_native_pools_donor','raptorlake_p_gtt_shadow_donor','raptorlake_p_pages_donor','raptorlake_p_caps_donor','raptorlake_p_caps_replay_donor','raptorlake_p_engine_donor','raptorlake_p_ring_image_donor','raptorlake_p_memory_donor','tgl_tlb_preparser','tgl_semaphore_tlb']]
if a.reference_source and a.accelerator:jobs += [('raptorlake_p_reference_recovery_adapter',[sys.executable,str(R/'tests/raptorlake_p_reference_recovery_adapter.py'),str(a.reference_source.resolve()),str(a.reference_framebuffer.resolve()),str(a.accelerator.resolve())],None)]
if a.reference_source:jobs += [('raptorlake_p_reference_tc_agdc_donor',[sys.executable,str(R/'tests/raptorlake_p_reference_tc_agdc.py'),str(a.reference_framebuffer.resolve())],None)]
if a.reference_source:jobs += [('raptorlake_p_reference_tc_rescan_donor',[sys.executable,str(R/'tests/raptorlake_p_reference_tc_rescan.py'),str(a.reference_framebuffer.resolve())],None)]
if a.reference_source:jobs += [('raptorlake_p_reference_metadata',[sys.executable,str(R/'tests/raptorlake_p_reference_metadata.py'),str(a.reference_source.resolve()),str(a.reference_framebuffer.resolve())],None)]
if a.reference_source:jobs += [('raptorlake_p_reference_refinement',[sys.executable,str(R/'tests/raptorlake_p_reference_refinement.py'),str(a.reference_source.resolve()),str(a.reference_framebuffer.resolve())],None)]
def check(job):
 n,compile,execute=job
 with (O/(n+'.log')).open('w') as log:
  rc=subprocess.run(compile,stdout=log,stderr=log).returncode
  if rc==0 and execute:rc=subprocess.run(execute,stdout=log,stderr=log).returncode
 return n,rc
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=dict(pool.map(check,jobs))
(O/'results.json').write_text(json.dumps(results,indent=2)+'\n')
failed={n:rc for n,rc in results.items() if rc};print(json.dumps({'passed':len(results)-len(failed),'failed':failed}))
sys.exit(bool(failed))
