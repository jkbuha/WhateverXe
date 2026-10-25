#!/usr/bin/env python3
"""Classify hibernation evidence without changing power or boot settings.

A bootloader preview or EXITBS marker does not establish a resumed kernel.
Inputs and raw captures should remain private. Output contains stage metadata,
not RTC/NVRAM encryption material. This tool never triggers sleep or a reboot.
"""
import argparse
import json
import re
from pathlib import Path


def decode_log(data):
    if data.startswith((b'\xff\xfe', b'\xfe\xff')):
        return data.decode('utf-16', errors='replace').replace('\x00', '')
    return data.decode('utf-8-sig', errors='replace').replace('\x00', '')


def loader_evidence(data):
    text = decode_log(data)
    detected = bool(re.search(r'\[EB\|H:IS\]\s+1\b', text))
    not_detected = bool(re.search(r'\[EB\|H:IS\]\s+0\b', text))
    read_started = '[EB|H:RDST]' in text
    read_complete = '[EB|H:RDEND]' in text
    exitbs = '[EB|LOG:EXITBS:START]' in text
    failed = '[EB|H:FAIL]' in text
    missing = sorted(set(re.findall(
        r'Err\(0xE\)\s*<-\s*RT\.GV\s+(boot-signature|boot-image-key)\b', text)))
    if read_complete and exitbs:
        stage = 'image_read_complete_and_exitbs_reached'
    elif read_complete:
        stage = 'image_read_complete'
    elif read_started:
        stage = 'image_read_started'
    elif failed:
        stage = 'loader_rejected_resume'
    elif detected:
        stage = 'hibernate_metadata_detected'
    else:
        stage = 'no_resume_attempt_proven'
    return {
        'furthest_loader_stage': stage,
        'hibernate_detected': detected,
        'hibernate_not_detected': not_detected,
        'image_read_started': read_started,
        'image_read_complete': read_complete,
        'exit_boot_services_reached': exitbs,
        'loader_failure_marker': failed,
        'missing_metadata_variables': missing,
        'mixed_detection_markers': detected and not_detected,
        'kernel_resume': 'unproven_by_bootloader_log',
        'display_resume': 'unproven_by_bootloader_log',
    }


def _positive_int(value):
    return type(value) is int and value > 0


def session_evidence(before, after):
    """Separate session continuity from cold boot and independent GPU evidence.

    memory_sentinel.live_verified must be supplied by a collector that verified
    the live process's PID, start identity and nonce, not by reading a stale file.
    Even live process continuity also happens after ordinary sleep or no sleep;
    genuine S4 needs separate image/power-off/loader evidence.
    """
    a = before.get('memory_sentinel') or {}
    b = after.get('memory_sentinel') or {}
    same = (
        isinstance(a.get('nonce'), str) and bool(a['nonce']) and
        a.get('nonce') == b.get('nonce') and
        isinstance(a.get('process_start'), str) and bool(a['process_start']) and
        a.get('process_start') == b.get('process_start') and
        _positive_int(a.get('pid')) and _positive_int(b.get('pid')) and a.get('pid') == b.get('pid') and
        type(a.get('counter')) is int and type(b.get('counter')) is int and
        a['counter'] >= 0 and b['counter'] > a['counter'] and b.get('live_verified') is True
    )
    old_uuid, new_uuid = before.get('boot_session_uuid'), after.get('boot_session_uuid')
    changed = bool(old_uuid and new_uuid and old_uuid != new_uuid)
    if same:
        session = 'live_process_continuity_observed'
    elif changed:
        session = 'new_boot_observed'
    else:
        session = 'unproven'
    old_gpu, new_gpu = before.get('gpu_exec'), after.get('gpu_exec')
    gpu_progress = None
    if same and type(old_gpu) is int and type(new_gpu) is int:
        gpu_progress = new_gpu > old_gpu
    return {
        'session': session,
        'boot_uuid_changed': changed,
        'live_memory_sentinel_continued': same,
        'gpu_completion_counter_advanced_in_continued_session': gpu_progress,
        'genuine_s4': 'requires_independent_power_off_image_and_loader_evidence',
        'physical_display': 'requires_operator_observation',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    loader = commands.add_parser('loader', help='Analyse each OpenCore log separately')
    loader.add_argument('logs', type=Path, nargs='+')
    session = commands.add_parser('session', help='Compare two captured state JSON files')
    session.add_argument('before', type=Path)
    session.add_argument('after', type=Path)
    args = parser.parse_args()
    inputs = args.logs if args.command == 'loader' else [args.before, args.after]
    for path in inputs:
        if path.stat().st_size > 16 * 1024 * 1024:
            parser.error('Evidence input exceeds 16 MiB; supply a boot log or state JSON, not a sleep image')
    if args.command == 'loader':
        result = [{'file': path.name, **loader_evidence(path.read_bytes())} for path in args.logs]
    else:
        result = session_evidence(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
