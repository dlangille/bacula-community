#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#   Bacula(R) - The Network Backup Solution

   Copyright (C) 2000-2022 Kern Sibbald

   The original author of Bacula is Kern Sibbald, with contributions
   from many others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   This notice must be preserved when any source code is
   conveyed and/or propagated.

   Bacula(R) is a registered trademark of Kern Sibbald.
#
# author: alain@baculasystems.com

import os
import sys
import argparse
import subprocess
import threading
import hashlib
import time
import queue
import codecs
import logging
import warnings
import stat
import re
import struct
import atexit

import bconsole

if True:
    import code, traceback, signal

    def debug(sig, frame):
        """Interrupt running process, and provide a python prompt for
        interactive debugging."""
        d={'_frame':frame}         # Allow access to frame object.
        d.update(frame.f_globals)  # Unless shadowed by global
        d.update(frame.f_locals)

        i = code.InteractiveConsole(d)
        message  = "Signal received : entering python shell.\nTraceback:\n"
        message += ''.join(traceback.format_stack(frame))
        i.interact(message)

    def dumpstacks(signal, frame):
        id2name = dict([(th.ident, th.name) for th in threading.enumerate()])
        code = []
        for threadId, stack in sys._current_frames().items():
            if id2name.get(threadId,"").startswith('pydevd.'):
                continue
            code.append("\n# %d Thread: %s(%d)" % (os.getpid(), id2name.get(threadId,""), threadId))
            for filename, lineno, name, line in traceback.extract_stack(stack):
                code.append('File: "%s", line %d, in %s' % (filename, lineno, name))
                if line:
                    code.append("  %s" % (line.strip()))
        print("\n".join(code), file=sys.stderr)

    def listen():
        signal.signal(signal.SIGUSR1, dumpstacks)  # Register handler

    listen()

def atexit_delete_file(paths):
    for path in paths:
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass

class Obj:
    pass

class DDE:
    BUCKETID_SHIFT=(64-16)
    BUCKETID_MASK=(0x7FFF<<BUCKETID_SHIFT)
    BUCKETIDX_MASK=(0xFFFF<<BUCKETID_SHIFT)

    CONTAINER_HEADER_SIZE=4096

    BLOCK_HEAD_COMPRESSED=(1<<31)
    BLOCK_HEAD_NOT_TRIED_COMPRESS=(1<<30)
    BLOCK_HEAD_SIZE_MASK=0x00FFFFFF
    BLOCK_HEAD_SIZE=4

    BUCKET_BAD_ADDR=0x0
    BUCKET_RAW_DATA=0x1 # data is stored inside the ref
    BUCKET_MISS_ADDR=0x2

    daemon_head='(?P<daemon>[^:]+): (?P<source>[^:]+):(?P<source_lineno>\d+)\s+'
    regex=Obj()
    regex.vacuum_start=re.compile(daemon_head+'Vacuum: (?P<start>\d{2}-\w{3}-\d{4} \d{2}:\d{2}:\d{2}) Start forceoptimize=(?P<forceoptimize>\d+) holepunching=(?P<holepunching>\d+) checkindex=(?P<checkindex>\d+) checkvolume=(?P<checkvolume>\d+)')
    regex.vacuum_end=re.compile(daemon_head+'Vacuum: (?P<end>\d{2}-\w{3}-\d{4} \d{2}:\d{2}:\d{2}) End\s+-+')
    regex.vacuum_bnum_min=re.compile(daemon_head+'Vacuum: bnum_min=(?P<bnum_min>\d+) bnum_max=(?P<bnum_max>\d+) mlock_max=(?P<mlock_max>\d+) mlock_strategy=(?P<mlock_strategy>\d+)')
    regex.vacuum_hole_size=re.compile(daemon_head+'Vacuum: hole_size=(?P<hole_size>\d+)')
    regex.vacuum_hash_count=re.compile(daemon_head+'Vacuum: hash_count=(?P<hash_count>\d+)/(?P<bnum>\d+) chunk_count=(?P<chunk_count>\d+) connections=(?P<connections>\d+)')
    regex.vacuum_preload=re.compile(daemon_head+'Vacuum: preload orphan count=(?P<preload_orphan>\d+) from (?P<preload_filename>\d+)')
    regex.vacuum_volumes=re.compile(daemon_head+'Vacuum: (?P<end_volume>\d{2}-\w{3}-\d{4} \d{2}:\d{2}:\d{2}) Number of volumes handled (?P<volume>\d+)/(?P<tot_volume>\d+) suspect_ref=(?P<suspect_ref_all_vols>\d+)')
    regex.vacuumbadref=re.compile(daemon_head+'VacuumBadRef (?P<type>\S+) FI=(?P<fi>\d+) SessId=(?P<sessid>\d+) SessTime=(?P<sesstime>\d+)( : ref\(\#(?P<hash>[0-9A-Fa-f]+) addr=(?P<addr>0x[0-9A-Fa-f]+) size=(?P<size>\d+)\) (?P<extra>.*)| : size=(?P<refsize>\d+)|)')
    regex.vacuum_optimize=re.compile(daemon_head+'Vacuum: (?P<optimize>\d{2}-\w{3}-\d{4} \d{2}:\d{2}:\d{2}) optimize_index count=(?P<optimize_count>\d+) del=(?P<optimize_>\d+) add=(?P<optimize_add>\d+) optimize_err=(?P<optimize_err>\d+)')
    regex.vacuum_need_optimize=re.compile(daemon_head+'Vacuum: need optimize, (?P<need_optimize>.*)')
    regex.vacuum_ref_count=re.compile(daemon_head+'Vacuum: ref_count=(?P<ref_count>\d+) error=(?P<error>\d+) suspect_ref=(?P<suspect_ref>\d+)')
    regex.vacuum_idxfix=re.compile(daemon_head+'Vacuum: idxfix=(?P<idxfix>\d+) 2miss=(?P<idxfix_2miss>\d+) orphan=(?P<idxfix_orphan>\d+) recoverable=(?P<recoverable>\d+)')
    regex.vacuum_idxmiss=re.compile(daemon_head+'Vacuum: 2miss=(?P<idxmfix_2miss>\d+) orphan=(?P<idxfix_orphan>\d+)')
    regex.vacuum_idxfix_err=re.compile(daemon_head+'Vacuum: idxupd_err=(?P<idxfix_err>\d+) chunk_read=(?P<chunk_read>\d+) chunk_read_err=(?P<chunk_read_err>\d+) chunkdb_err=(?P<chunkdb_err>\d+)')

    vacuum_vars='error,suspect_ref,idxfix,idx_2miss,idxmiss_2miss,recoverable,orphan,suspect_ref,idxfix_err,chunk_read,chunk_read_err,chunk_write_err,chunkdb_err,idxmiss_2miss'.split(',')
    orphan_struct=struct.Struct('!Q')

    regex.dedup_usage=re.compile("""\
Dedupengine status:\s+"(?P<dedupengine_name>.*)"
\s+DDE:\s+hash_count=(?P<hash_count>\d+)\s+ref_count=(?P<ref_count>\d+)\s+ref_size=(?P<ref_size>\d+(\.\d*)?)\s(?P<ref_size_unit>[KMGTPEZY]?B)
\s+ref_ratio=(?P<ref_ratio>\d+(\.\d*)?)\s+size_ratio=(?P<size_ratio>\d+(\.\d*)?)\s+dde_errors=(?P<dde_errors>\d+)
\s+Config:\s+bnum=(?P<bnum>\d+)\s+bmin=(?P<bmin>\d+)\s+bmax=(?P<bmax>\d+)\s+mlock_strategy=(?P<mlock_strategy>\d+)\s+mlocked=(?P<mlocked_mb>\d+)MB\s+mlock_max=(?P<mlock_max_mb>\d+)MB(\\n\s+mlock_error="(?P<mlock_error>.*)")?
\s+HolePunching:\s+hole_size=(?P<hole_size_kb>\d+)\s+KB
\s+Containers:\s+chunk_allocated=(?P<chunk_allocated>\d+)\s+chunk_used=(?P<chunk_used>\d+)
\s+disk_space_allocated=(?P<disk_space_allocated>\d+(\.\d*)?)\s+(?P<disk_space_allocated_unit>[KMGTPEZY]?B)\s+disk_space_used=(?P<disk_space_used>\d+(\.\d*)?)\s+(?P<disk_space_used_unit>[KMGTPEZY]?B)\s+containers_errors=(?P<containers_errors>\d+)
\s+Vacuum:\s+last_run="(?P<vac_last_run>.*)"\s+duration=(?P<vac_duration>\d+)s\s+ref_count=(?P<vac_ref_count>\d+)\sref_size=(?P<vac_ref_size>\d+(\.\d*)?)\s+(?P<vac_ref_size_unit>[KMGTPEZY]?B)
\s+vacuum_errors=(?P<vac_errors>\d+)\s+orphan_ref=(?P<vac_orphan_ref>\d+)\s+suspect_ref=(?P<vac_suspect_ref>\d+)\s+(progress=(?P<vac_progress>\d+)%%)?
\s+Scrubber:\s+last_run="(?P<scrub_last_run>.*)".*
\s+Stats:\s+read_chunk=(?P<read_chunk>\d+)\s+query_hash=(?P<query_hash>\d+)\s+new_hash=(?P<new_hash>\d+)\s+calc_hash=(?P<calc_hash>\d+)
""", re.M)

    def __init__(self, dedup_dir, dedup_index_dir):
        self.dedup_dir=dedup_dir
        self.dedup_index_dir=dedup_index_dir
        self.archdir=self.dedup_index_dir
        self.vacuum_orphan_lst_path=os.path.join(self.archdir, "orphanaddr.bin")

    def GetDedupDir(self):
        return self.dedup_dir

    def GetMetaDir(self):
        return self.dedup_index_dir

    def GetFSMPath(self):
        return os.path.join(self.dedup_index_dir, 'bee_dde.idx')

    def GetIndexPath(self):
        return os.path.join(self.dedup_index_dir, 'bee_dde.tch')

    def SaveOrphan(self, orphans, appending=False):
        f=open(self.vacuum_orphan_lst_path, 'ab' if appending else 'wb')
        for orphan in orphans:
            f.write(self.orphan_struct.pack(orphan))
        f.close()

    def ChunkAddr2ContainerId(addr):
        return (addr & self.BUCKETID_MASK)>>self.BUCKETID_SHIFT

    def ChunkAddr2Idx(addr):
        return (addr & ~self.BUCKETIDX_MASK)

    def CalcChunkAddr(self, containerid, containeridx):
        return containerid<<self.BUCKETID_SHIFT | containeridx

    def ZapContainers(self):
        root_dir=self.GetDedupDir()
        for filename in os.listdir(root_dir):
            if filename.startswith('bee_dde') and filename.endswith('.blk'):
                path=os.path.join(root_dir, filename)
                filesize=os.path.getsize(path)
                os.truncate(path, self.CONTAINER_HEADER_SIZE)
                assert(os.path.getsize(path)==self.CONTAINER_HEADER_SIZE)
                os.truncate(path, filesize)

    def GetContainerPath(self, container_id):
        root_dir=self.GetDedupDir()
        filename='bee_dde%04d.blk' % (container_id,)
        return os.path.join(root_dir, filename)

    def ReadVacuum(self, tracefile, checkerr0=None, rewind=False):
        vacuum_start=0
        expected=dict()
        values=dict()
        start=tracefile.Tell()
        found=tracefile.Search([ self.regex.vacuum_start, self.regex.vacuum_end,
            self.regex.vacuum_bnum_min, self.regex.vacuum_hole_size, self.regex.vacuum_hash_count,
            self.regex.vacuum_preload, self.regex.vacuum_volumes, self.regex.vacuum_ref_count,
            self.regex.vacuum_idxfix, self.regex.vacuum_idxfix_err, self.regex.vacuum_optimize,
            self.regex.vacuum_need_optimize, self.regex.vacuum_idxmiss, ])
        val=dict()
        err=dict()
        if checkerr0!=None:
            for k in self.vacuum_vars:
                val[k]='0'
            val.update(checkerr0)
        for m in found:
            values.update(m.groupdict())
            if m.re==self.regex.vacuum_start:
                vacuum_start+=1
            for k in self.vacuum_vars:
                try:
                    v=m.group(k)
                except IndexError:
                    pass
                else:
                    if checkerr0!=None and val[k]!=None and v!=val[k]:
                        err[k]=v
        if rewind:
            tracefile.Seek(start)
        assert not err, "Vacuum error counter not to zero values : %r" % (err, )
        assert vacuum_start==1 and 'start' in values and 'end' in values, 'count=%d start=%r end=%r' % (vacuum_start, 'start' in values, 'end' in values)
        return values

class FileReader:
    def __init__(self, filename):
        self.filename=filename
        self.file=None
        self.lineno=None

    def Open(self):
        if self.file==None:
            try:
                self.file=open(self.filename, 'rt')
                self.lineno=0
            except FileNotFoundError:
                pass
        return self.file

    def Search(self, regex, pos=None):
        """search the file for any line matching regex
        if pos is None then continue searching from last search"""
        found=[]
        if self.Open():
            if isinstance(regex, str):
                regex=re.compile(regex)
            if isinstance(regex, re._pattern_type):
                regex=[regex, ]
            assert isinstance(regex, list), "expect a list of regex"
            if pos!=None:
                self.lineno, p=pos
                self.file.seek(p)
            for line in self.file:
                self.lineno+=1
                line=line.rstrip('\n')
                for r in regex:
                    # print(line, r.pattern, r.match(line))
                    match=r.match(line)
                    if match:
                        found.append(match)
                        continue
        return found

    def Tell(self):
        """return the current position in the file"""
        return (self.lineno, self.file.tell()) if self.file else None

    def Seek(self, pos):
        if pos!=None and self.Open():
            self.lineno, p=pos
            self.file.seek(p)

class Daemon:

    short=None
    def __init__(self, lab, name):
        self.lab=lab
        self.name=name
        self.trace=FileReader(os.path.join(self.lab.vars.working, '%s.trace' % (self.name, )))

    def Start(self):
        self.lab.Shell('$bin/bacula-ctl-%s start' % (self.short, ))

    def Stop(self):
        self.lab.Shell('$bin/bacula-ctl-%s stop' % (self.short, ))

    def Restart(self):
        self.Stop()
        self.Restart()

class Storage(Daemon):
    short='sd'
    def __init__(self, lab, name):
        super(Storage, self).__init__(lab, name)
        self.dde=DDE(os.path.join(self.lab.vars.working, 'dde'), os.path.join(self.lab.vars.working, 'dde'))

class Client(Daemon):
    short='fd'
    def __init__(self, lab, name):
        super(Client, self).__init__(lab, name)

class Director(Daemon):
    short='dir'
    def __init__(self, lab, name):
        super(Director, self).__init__(lab, name)


class Shell:

    def __init__(self, shell='/bin/sh', verbose=True):
        self.queue_out=queue.Queue()
        self.stdout_verbose=False
        self.proc=subprocess.Popen([ shell, ], stdin=subprocess.PIPE, stdout=None if verbose else subprocess.DEVNULL, stderr=None if verbose else subprocess.DEVNULL)
        self.output=[ None, None, None ]
        # ${tmp} is 'unknown' and not cleaned up yet, use '/tmp'
        tmp='/tmp'
        self.pipeout_name=os.path.join(tmp, 'stdout%d' % (os.getpid(), ))
        self.pipeerr_name=os.path.join(tmp, 'stderr%d' % (os.getpid(), ))
        atexit.register(atexit_delete_file, [self.pipeout_name, self.pipeerr_name])
        for path in [self.pipeout_name, self.pipeerr_name]:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass
            os.mkfifo(path)

    def __del__(self):
        for path in [self.pipeout_name, self.pipeerr_name]:
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass

    def Exec(self, cmd, verbose=False):
        """run cmd in the shell and return exit code
        cmd can be "unsafe" (see ExecOut())
        guess the end of the process by running an echo into a PIPE
        after the cmd.
        if 'exit XX' is called then call sys.exit(XX)
        """
        cmd2='%s\n> %s echo $?\n' % (cmd.rstrip('\n'), self.pipeout_name)
        self.proc.stdin.write(codecs.encode(cmd2))
        self.proc.stdin.flush()

        fd=os.open(self.pipeout_name, os.O_RDONLY|os.O_NONBLOCK)
        buf=b''
        while buf==b''and self.proc.poll()==None:
            time.sleep(0.1)
            buf=os.read(fd, 4096)
        if buf==b'':
            os.close(fd)
            sys.exit(self.proc.returncode)
        else:
            data=b''
            while buf:
                data+=buf
                buf=os.read(fd, 4096)
            os.close(fd)
        return int(data)

    def ReadOut(self, pipe_name, out_idx):
        fd=os.open(pipe_name, os.O_RDONLY)
        data=b''
        buf=os.read(fd, 4096)
        while buf:
            data+=buf
            buf=os.read(fd, 4096)
        os.close(fd)
        self.output[out_idx]=data

    def ExecOut(self, cmd):
        """return [code, "stdout", "stderr"]
        cmd must be a "safe" command that don't let any open background process
        writing to stout or stderr. Use Exec() instead
        """

        # start 2 threads that read two fifos for stdout and stderr
        self.output=[ None, None, None ]
        thread_stdout=threading.Thread(target=self.ReadOut, args=(self.pipeout_name, 1))
        thread_stdout.start()
        thread_stderr=threading.Thread(target=self.ReadOut, args=(self.pipeerr_name, 2))
        thread_stderr.start()
        # redirect 
        cmd2='(%s) >%s 2>%s\n' % (cmd.rstrip('\n'), self.pipeout_name, self.pipeerr_name)

        self.proc.stdin.write(codecs.encode(cmd2))
        self.proc.stdin.flush()
        thread_stdout.join()
        thread_stderr.join()

        # get the error code
        cmd2='> %s echo $?\n' % (self.pipeout_name, )
        self.proc.stdin.write(codecs.encode(cmd2))
        self.proc.stdin.flush()
        fd=os.open(self.pipeout_name, os.O_RDONLY)
        data=b''
        buf=os.read(fd, 4096)
        while buf:
            data+=buf
            buf=os.read(fd, 4096)
        os.close(fd)
        return int(data), codecs.decode(self.output[1].rstrip(b'\n')), codecs.decode(self.output[2])

    def Close(self):
        if self.proc:
            self.proc.stdin.close()
            self.proc.wait()
            self.proc=None

    def __del__(self):
        self.Close()

class Lab:
    variables='cwd db_name db_user db_password working dumps bin FORCE_DEDUP' \
              ' DEDUP_FS_OPTION DEDUP_FD_CACHE scripts conf rscripts tmp src' \
              ' tmpsrc bperl CLIENT AUTOCHANGER_SCRIPT out'

    DDE=DDE

    def __init__(self, testname=None, profile='dedup-simple', profile_args=None, shell=False, cleanup=None, argv=None):
        assert shell, "shell must be always enable while in mixed python/shell mode"
        self.args = mainparser.parse_args(argv if argv else None)
        self.cleanup=cleanup if cleanup!=None else self.args.cleanup

        self.testname=testname if testname else os.path.basename(sys.argv[0])
        self.profile=profile
        self.profile_args=profile_args if profile_args else dict()
        if shell==True:
            shell='/bin/sh'
        self.debug=not (os.getenv('REGRESS_DEBUG')!='1')
        self.verbose=self.args.verbose or self.debug
        if self.args.debug:
            self.debug=True
            self.verbose=True
        self.shell=Shell(shell=shell, verbose=True)
        atexit.register(self.AtExit)
        self.shell.Exec('TestName="%s"' % (self.testname, ))
        self.shell.Exec('BLAB_PY=1')
        self.shell.Exec('. scripts/functions')
        self.vars0=dict()
        self.vars=Obj()
        for var_name in self.variables.split():
            _code, value, err=self.shell.ExecOut('echo ${%s}' % (var_name, ))
            self.SetVar0(var_name, value)

        if self.cleanup:
            self.Cleanup()

        self.SetupProfile()

        self.ExportVar('QUERY_DDE_ADVANCED', '1') # enable advanced function for query_dde 
        self.bconsole=bconsole.BConsole(regress=True)

        self.dir=Director(self, '127.0.0.1-dir')
        self.sd=Storage(self, '127.0.0.1-sd')
        self.fd=Client(self, '127.0.0.1-fd')

    def AtExit(self):
        self.shell.Close()

    def SetVar0(self, var_name, value):
        self.vars0[var_name]=value
        setattr(self.vars, var_name, value)

    def SetVar(self, var_name, value):
        self.SetVar0(var_name, value)
        self.shell.Exec("%s='%s'" % (var_name, value))

    def ExportVar(self, var_name, value):
        self.SetVar0(var_name, value)
        self.shell.Exec("export %s='%s'" % (var_name, value))

    def GetVar(self, var_name, default=None):
        return self.vars0.get(var_name, default)

    def Shell(self, cmd):
        return self.shell.Exec(cmd)

    def ShellOut(self, cmd, codes=[ 0 ]):
        c, out, err=self.shell.ExecOut(cmd)
        if codes!=None and not c in codes:
            raise RuntimeError('Unexpected return code %d for command "%s" out=%r err=%r' % (c, cmd, out, err))
        return c, out, err

    def BconsoleScript(self, script, expansion=True, verbose=None):
        if expansion:
            script=script.format(**self.vars0)
        if verbose==None:
            verbose=self.debug
        returncode, out, err=self.bconsole.rawrun(script, verbose=verbose)
        return returncode, out, err

    def BconsoleScriptOut(self, script, expansion=True, verbose=None):
        returncode, out, err=self.BconsoleScript(script, expansion, verbose)
        return returncode, codecs.decode(out), codecs.decode(err)

    def Cleanup(self):
        return self.Shell('scripts/cleanup')

    def CheckConfig(self, force, *args, **kwargs):
        """check that FORCE_(ALIGNED|DEDUP|SDCALLS|CLOUD)"""
        if force in ('FORCE_ALIGNED', 'FORCE_DEDUP', 'FORCE_SDCALLS', 'FORCE_CLOUD'):
            if self.GetVar(force)!='yes':
                self.Exit(0, " {}!=yes, skip test  {}".format(force, self.testname))
            if force=='FORCE_DEDUP' and len(args)>0:
                dedup_options=args[0]
                if not isinstance(dedup_options, (list, tuple)):
                    dedup_options=[dedup_options ]
                dedup_option=self.GetVar('DEDUP_FS_OPTION')
                if dedup_option not in dedup_options:
                    self.Exit(0, "DEDUP_FS_OPTION don't match, skip test {}".format(self.testname))
        else:
            self.Die("Unknow config argument {}".format(force))

    def StartTest(self):
        if self.cleanup:
            self.Shell('start_test')
        else:
            self.Shell('reset_test')
        self.StartBacula()

    def EndTest(self):
        self.StopBacula()
        self.Shell('end_test')
        self.shell.Close()

    def StartBacula(self):
        open(os.path.join(self.vars.tmp, 'bconcmds'), 'w').write('quit\n')
        self.Shell('run_bacula')

    def StopBacula(self):
        self.Shell('stop_bacula')

    def MakePlugin(self):
        warnings.warn("deprecated", DeprecationWarning)
        self.MakePlugins('fd:test-dedup')

    def MakePlugins(self, lst):
        """expect a list of plugins like [ 'fd:test-dedup' ]""" 
        if not isinstance(lst, list):
            lst=[ lst ]
        _code, pwd, _err=self.shell.ExecOut('pwd')
        for pl in lst:
            daemon, plugin=pl.split(':')
            self.shell.Exec('cd ${cwd}/build/src/plugins/'+daemon)
            self.shell.Exec('make')
            self.shell.Exec('make install-'+plugin)
        self.shell.Exec('cd '+pwd)

    def GetVolume(self, volname):
        volume=dict()
        volume['name']=volname
        volume['path']=os.path.join(self.vars.tmp, volname)
        return volume

    def s2usize(self, val, unit, comment=""):
        units=dict(B=1, KB=1E3, MB=1E6, GB=1E9, TB=1E12, EB=1E15, ZB=1E18, YB=1E21)
        val=val.translate({44:None})
        try:
            v=float(val)*units[unit]
        except Exception as ex:
            raise Exception('error conversing %r %r (%r)' % (val, unit, comment))
        return v

    def s2time(self, val, comment=""):
        try:
            v=val # TODO
        except Exception as ex:
            raise Exception('error conversing %r %r (%r)' % (val, unit, comment))
        return v

    def s2int(self, val):
        return int(val.translate({44:None}))

    def DedupUsage(self, storage=None):
        if storage==None:
            storage=self.GetVar('STORAGE')
        returncode, out, err=self.BconsoleScriptOut('dedup usage storage=%s' % (storage,))
        pos=out.find('Dedupengine status:')
        self.Assert(pos!=-1, "dedup usage not found\n%s\n%s" % (out, err))
        st=out[pos:]
        match=DDE.regex.dedup_usage.match(st)
        if not match:
            """embeded debugging"""
            regex=''
            for line in DDE.regex.dedup_usage.pattern.split('\n'):
                if regex:
                    regex+='\n'+line
                else:
                    regex=line

                self.Log(logging.ERROR, "line=%r", line)
                if not re.match(regex, st, re.M):
                    self.Log(logging.ERROR, "%s", st)
                    self.Log(logging.ERROR, "dedup usage regex fail here:")
                    self.Log(logging.ERROR, "%r", line)
                    break

        self.Assert(match, "dedup usage regex don't match:\n%s" % (st, ))
        values=match.groupdict()
        convert=dict(hash_count=int, ref_count=int, ref_size=self.s2usize,
             ref_ratio=float, size_ratio=float, dde_errors=int,
             bnum=int, bmin=int, bmax=int, mlock_strategy=int, mlocked_mb=int, mlock_max_mb=int,
             hole_size_kb=int,
             chunk_allocated=int, chunk_used=int,
             disk_space_allocated=self.s2usize, disk_space_used=self.s2usize, containers_errors=int,
             vac_last_run=self.s2time, vac_duration=int, vac_ref_count=int, vac_ref_size=self.s2usize,
             vac_errors=int, vac_orphan_ref=int, vac_suspect_ref=int,
             read_chunk=int, query_hash=int, new_hash=int, calc_hash=int)
        for k in convert:
            if convert[k]==self.s2usize:
                values[k]=self.s2usize(values[k], values[k+'_unit'])
            else:
                if False and k in ('bmin', 'bmax'):
                    self.Log(logging.INFO, '%s=%r -> %r', k, values[k], convert[k](values[k]))
                values[k]=convert[k](values[k])

        return values

    def DedupSetMaximumContainerSize(self, mcs):
        lab.Shell("""$bperl -e 'add_attribute("$conf/bacula-sd.conf", "MaximumContainerSize", "{}B", "Storage")'""".format(mcs))

    def Assert(self, cond, message="Unexpected error"):
        if not cond:
            raise Exception(message)

    def SetupProfile(self):
        if self.profile not in ('dedup-simple', 'dedup-autochanger'):
            self.Log(loggin.CRITICAL, "unknown profile: %s", self.profile)

        if self.profile.startswith('dedup-'):
            if 'MaximumContainerSize' in self.profile_args:
                self.SetVar('DEDUP_MAXIMUM_CONTAINER_SIZE', '{}KB'.format(self.profile_args['MaximumContainerSize']//1024))
            if self.cleanup:
                self.SetupDedup()

        if self.profile=='dedup-simple':
            if self.cleanup:
                self.Shell('scripts/copy-plugin-confs')
            self.SetVar('STORAGE', 'File')
        elif self.profile=='dedup-autochanger':
            if self.cleanup:
                self.Shell('scripts/copy-dedup-confs')
            self.SetVar('STORAGE', 'DiskChanger')

        if self.profile in ('dedup-simple', 'dedup-autochanger'):
            self.SetVar('JobName', 'DedupPluginTest')
            if self.cleanup:
                self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Maximum Concurrent Jobs", "10", "Director")'""")
                self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Maximum Concurrent Jobs", "10", "Client")'""")
                self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Maximum Concurrent Jobs", "10", "Storage", "File")'""")
                self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Maximum Concurrent Jobs", "10", "Job", "$JobName")'""")
                self.Shell("""$bperl -e 'add_attribute("$conf/bacula-sd.conf", "Volume Poll Interval", "5s", "Device", "FileStorage")'""")
                #self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Media Type", "File", "Storage", "File1")'""")
                #self.Shell("""$bperl -e 'add_attribute("$conf/bacula-dir.conf", "Maximum Concurrent Jobs", "10", "Storage", "File1")'""")
                #self.Shell("""$bperl -e 'add_attribute("$conf/bacula-sd.conf", "Media Type", "File", "Device", "FileStorage1")'""")

        if self.profile.startswith('dedup-'):
            self.ResetDedup()

    def SetupDedup(self):
        self.Shell('check_dedup_enable')
        self.Shell('require_query_dde')
        #self.MakePlugins([ 'fd:test-dedup', ])
        # create 'select-cfg.sh' file to use in bconsole command like this :
        # @exec "{tmp}/select-cfg.sh 0"
        # run job=DedupPluginTest level=Full storage=File1 yes
        # @exec "{tmp}/select-cfg.sh next"
        f=open(os.path.join(self.vars.tmp, 'select-cfg.sh'), 'wt')
        f.write("""\
# The plugin unlink the file just after open()
# echo "$$ `date` $@" >> /tmp/log

# if inotifywait is installed then use it to "instantly" detect that the file
# as been deleted from the direcory (aka "consumed") else wait 1s
wait_cmd="`which inotifywait` -t 5 -e delete {tmp}" || wait_cmd="sleep 1"
while [ -f {tmp}/dedupstreams.conf ] ; do $wait_cmd; done
for var in "$@" ; do
   if [ "${{var}}" = "next" ] ; then
      var=0
      if [ -f {tmp}/dedupstreams.last ] ; then
         var=`cat {tmp}/dedupstreams.last`
         var=$(($var + 1))
      fi
   fi
   echo {tmp}/stream${{var}}.dedup >> {tmp}/dedupstreams.conf
   chmod a+x {tmp}/dedupstreams.conf # let the plugin does the unlink
   echo ${{var}} > {tmp}/dedupstreams.last
done
# echo "$$ `date` done" >> /tmp/log
""".format(tmp=self.vars.tmp))
        os.fchmod(f.fileno(), stat.S_IRUSR|stat.S_IWUSR|stat.S_IXUSR|stat.S_IRGRP|stat.S_IXGRP|stat.S_IROTH|stat.S_IXOTH)
        f.close()

        # create a default 'stream0.dedup' file
        f=open(os.path.join(self.vars.tmp, 'stream0.dedup'), 'wt')
        f.write("""\
global_size=102400M
chunk_min_size=3K
chunk_max_size=64K
deviation=10
seed=1234
size=128M
start=-1
""")
        f.close()

    def ResetDedup(self):
        # cleanup 'dedupstreams.conf'
        try:
            os.unlink(os.path.join(self.vars.tmp, 'dedupstreams.conf'))
        except FileNotFoundError:
            pass

    def ListMedia(self, type=None, status=None):
        """typ & status can be list"""
        convert=dict(MediaId=self.s2int, VolumeName=str, MediaType=str, VolBytes=self.s2int, \
                     VolStatus=str, )
        returncode, out, err=self.BconsoleScriptOut('llist media\n', verbose=False)
        media=None
        medias=dict()
        skipping_garbadge=True
        for line in out.splitlines(False):
            if skipping_garbadge:
                skipping_garbadge=not line.startswith('Using Catalog')
                continue
            line=line.strip()
            if line:
                try:
                    k, v=line.split(':', 1)
                except:
                    if line not in ('No results to list.', 'You have messages.'):
                        print(out)
                        print("=> {!r}".format(line), file=sys.stderr)

                try:
                    conv=convert[k]
                except:
                    continue
                val=conv(v.strip())
                if media:
                    setattr(media, k, val)
                if k=='MediaId':
                    media=Obj()
                    media.MediaId=val
                    medias[val]=media
                elif k=='MediaType':
                    if type!=None and val not in type and media.MediaId in medias:
                        del medias[media.MediaId]
                        continue
                elif k=='VolStatus':
                    if status!=None and val not in status and media.MediaId in medias:
                        del medias[media.MediaId]
                        continue
            else:
                media=None
        return medias

    def ListJobs(self, type=None, status=None):
        """typ & status can be a string"""
        convert=dict(JobId=self.s2int, Name=str, Type=str, Level=str, JobStatus=str, \
                     JobErrors=self.s2int,)
        returncode, out, err=self.BconsoleScriptOut('llist jobs\n', verbose=False)
        job=None
        jobs=dict()
        skipping_garbadge=True
        for line in out.splitlines(False):
            if skipping_garbadge:
                skipping_garbadge=not line.startswith('Using Catalog')
                continue
            line=line.strip()
            if line:
                try:
                    k, v=line.split(':', 1)
                except:
                    if line not in ('No results to list.', 'You have messages.'):
                        print(out)
                        print("=> {!r}".format(line), file=sys.stderr)

                try:
                    conv=convert[k]
                except:
                    continue
                val=conv(v.strip())
                if job:
                    setattr(job, k, val)
                if k=='JobId':
                    job=Obj()
                    job.JobId=val
                    jobs[val]=job
                elif k=='Type':
                    if type!=None and val not in type and job.JobId in jobs:
                        del jobs[job.JobId]
                        continue
                elif k=='JobStatus':
                    if status!=None and val not in status and job.JobId in jobs:
                        del jobs[job.JobId]
                        continue
            else:
                job=None
        return jobs

    def GetJob(self, jobid, type=None):
        """use -1 for last jobid"""
        jobs=self.ListJobs(type=type)
        if not jobs:
            return None
        if jobid<0:
            if jobid==-1:
                jobid=max(jobs.keys())
            else:
                s=sorted(list(jobs.keys()))
                jobid=s[jobid]
        return jobs[jobid]

    def AssertNoJobError(self, exclude=None, type=None):
        """exclude is a list of job to that can be in error"""
        jobs=self.ListJobs(type=type)
        errors=[]
        for jobid in jobs:
            job=jobs[jobid]
            if exclude!=None and jobid in exclude:
                continue
            if job.JobStatus not in 'T' or job.JobErrors!=0:
                self.Log("job %d status=%s errors=%d", jobid, job.JobStatus, job.JobErrors)
                errors.append(str(job.JobId))
        self.Assert(len(errors)==0, "%d jobs have errors: %s" % (len(errors), ','.join(errors)))

    def Log(self, lvl, msg, *args, **kwargs):
        assert not kwargs, "dont handle this"
        if lvl>logging.WARNING or self.debug or (lvl>=logging.INFO and self.verbose):
            print(msg % args, file=sys.stderr)
            sys.stderr.flush()
            if lvl>=logging.CRITICAL:
                sys.exit(1)

    def Exit(self, code, msg=None):
        if msg:
            self.Log(logging.INFO, msg)
        sys.exit(code)

    def Die(self, msg=None):
        if msg:
            self.Log(logging.CRITICAL, msg)
        else:
            sys.exit(1)


class EZThread(threading.Thread):
    cont=True  # Continue
    stopping=False

    def AsyncStop0(self):
        pass

    def AsyncStop(self):
        if not self.stopping:
            self.AsyncStop0()
            self.stopping=True
            self.cont=False

    def Join(self):
        self.AsyncStop()
        self.join()


mainparser=argparse.ArgumentParser(description='Bacula regression test')
mainparser.add_argument('--debug', action='store_true', help="idem REGRESS_DEBUG=1")
mainparser.add_argument('--verbose', action='store_true', help="be verbose")
mainparser.add_argument('--cleanup', action='store_true', help="don't reset data", default=True)
mainparser.add_argument('--no-cleanup', dest='cleanup', action='store_false', help="don't reset data")

if __name__ == "__main__":

    s=Shell()
    out=s.ExecOut('echo hello')
    print('--> %r' % (out,))
    out=s.ExecOut('ls /dontexist')
    print('--> %r' % (out,))
    out=s.ExecOut('sleep 1')
    print('--> %r' % (out,))
    out=s.ExecOut('false')
    print('--> %r' % (out,))
    s.Exec('ls /tmp')
    s.Close()
