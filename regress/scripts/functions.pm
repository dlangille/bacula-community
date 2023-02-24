################################################################
use strict;

=head1 LICENSE

   Bacula(R) - The Network Backup Solution

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

=cut

package scripts::functions;
use File::Basename qw/basename/;
# Export all functions needed to be used by a simple 
# perl -Mscripts::functions -e '' script
use Exporter;
our @ISA = qw(Exporter);

our @EXPORT = qw(update_some_files create_many_files check_multiple_copies
                  update_client $HOST $BASEPORT add_to_backup_list
                  run_bconsole run_bacula start_test end_test create_bconcmds
                  create_many_dirs cleanup start_bacula $FORCE_DEDUP $DEDUP_FD_CACHE
                  $DEDUP_FS_OPTION get_dirname check_jobmedia_content
                  stop_bacula get_resource set_maximum_concurrent_jobs get_time
                  add_attribute check_prune_list check_min_volume_size
                  init_delta update_delta check_max_backup_size comment_out
                  create_many_files_size check_jobmedia  $plugins debug p
                  check_max_volume_size $estat $bstat $rstat $zstat $cwd $bin
                  $scripts $conf $rscripts $tmp $working $dstat extract_resource
                  $db_name $db_user $db_password $src $tmpsrc $out $CLIENT docmd
                  set_global_maximum_concurrent_jobs check_volumes update_some_files_rep
                  remote_init remote_config remote_stop remote_diff remote_check check_vacuum_badref
                  get_field_size get_field_ratio create_binfile get_bytes get_mbytes get_mbytes_md5
                  create_rconsole check_parts create_scratch_pool create_counter 
                  check_maxpoolbytes check_maxpoolbytes_from_file check_json_tools
                  check_aligned_volumes check_aligned_data  check_tcp check_tcp_loop
                  setup_fd_encryption check_encryption setup_collector get_attribute
                  setup_fdcallsdir setup_tls setup_fd_tls setup_sd_tls setup_dir_tls setup_cons_tls
                  check_openfile check_cloud_hash check_bscan add_log_message compare_backup_content
                  check_tls_traces println add_virtual_changer check_events check_events_json
                  create_many_hardlinks check_dot_status parse_fuse_trace generate_random_seek
                 check_storage_selection check_json get_perm check_protect
                 get_running_jobs get_client_name
);


use File::Copy qw/copy/;

our ($cwd, $bin, $scripts, $conf, $rscripts, $tmp, $working, $estat, $dstat,
     $plugins, $bstat, $zstat, $rstat, $debug, $out, $TestName, $FORCE_ALIGNED,
     $PREBUILT, $FORCE_CLOUD, $FORCE_DEDUP, $DEDUP_FD_CACHE, $DEDUP_FS_OPTION,
     $REMOTE_CLIENT, $REMOTE_ADDR, $REMOTE_FILE, $REMOTE_PORT, $REMOTE_PASSWORD,
     $REMOTE_STORE_ADDR, $REGRESS_DEBUG, $REMOTE_USER, $start_time, $end_time,
     $db_name, $db_user, $db_password, $src, $tmpsrc, $HOST, $BASEPORT, $CLIENT);

END {
    if ($estat || $rstat || $zstat || $bstat || $dstat) {
        exit 1;
    }
}

BEGIN {
    # start by loading the ./config file
    my ($envar, $enval);
    if (! -f "./config") {
        die "Could not find ./config file\n";
    }
    # load the ./config file in a subshell doesn't allow to use "env" to display all variable
    open(IN, ". ./config; set |") or die "Could not run shell: $!\n";
    while ( my $l = <IN> ) {
        chomp ($l);
        if ($l =~ /^([\w\d]+)='?([^']+)'?/) {
            next if ($1 eq 'SHELLOPTS'); # is in read-only
            ($envar,$enval) = ($1, $2);
            $ENV{$envar} = $enval;
        }
    }
    close(IN);
    $cwd = `pwd`; 
    chomp($cwd);

    # set internal variable name and update environment variable
    $ENV{db_name}     = $db_name     = $ENV{db_name}     || 'regress';
    $ENV{db_user}     = $db_user     = $ENV{db_user}     || 'regress';
    $ENV{db_password} = $db_password = $ENV{db_password} || '';

    $ENV{bin}      = $bin      =  $ENV{bin}      || "$cwd/bin";
    $ENV{tmp}      = $tmp      =  $ENV{tmp}      || "$cwd/tmp";
    $ENV{src}      = $src      =  $ENV{src}      || "$cwd/src";
    $ENV{conf}     = $conf     =  $ENV{conf}     || $bin;
    $ENV{scripts}  = $scripts  =  $ENV{scripts}  || $bin;
    $ENV{plugins}  = $plugins  =  $ENV{plugins}  || "$bin/plugins";
    $ENV{tmpsrc}   = $tmpsrc   =  $ENV{tmpsrc}   || "$cwd/tmp/build";
    $ENV{working}  = $working  =  $ENV{working}  || "$cwd/working";    
    $ENV{rscripts} = $rscripts =  $ENV{rscripts} || "$cwd/scripts";
    $ENV{HOST}     = $HOST     =  $ENV{HOST}     || "localhost";
    $ENV{BASEPORT} = $BASEPORT =  $ENV{BASEPORT} || "8101";
    $ENV{REGRESS_DEBUG} = $debug         = $ENV{REGRESS_DEBUG} || 0;
    $ENV{REMOTE_CLIENT} = $REMOTE_CLIENT = $ENV{REMOTE_CLIENT} || 'remote-fd';
    $ENV{REMOTE_ADDR}   = $REMOTE_ADDR   = $ENV{REMOTE_ADDR}   || undef;
    $ENV{REMOTE_FILE}   = $REMOTE_FILE   = $ENV{REMOTE_FILE}   || "/tmp";
    $ENV{REMOTE_PORT}   = $REMOTE_PORT   = $ENV{REMOTE_PORT}   || 9102;
    $ENV{REMOTE_PASSWORD} = $REMOTE_PASSWORD = $ENV{REMOTE_PASSWORD} || "xxx";
    $ENV{REMOTE_STORE_ADDR}=$REMOTE_STORE_ADDR=$ENV{REMOTE_STORE_ADDR} || undef;
    $ENV{REMOTE_USER}   = $REMOTE_USER   = $ENV{REMOTE_USER}   || undef;
    $ENV{FORCE_ALIGNED} = $FORCE_ALIGNED = $ENV{FORCE_ALIGNED} || 'no';
    $ENV{FORCE_CLOUD}   = $FORCE_CLOUD = $ENV{FORCE_CLOUD} || 'no';
    $ENV{FORCE_DEDUP}     = $FORCE_DEDUP = $ENV{FORCE_DEDUP} || 'no';
    $ENV{PREBUILT}      = $PREBUILT = $ENV{PREBUILT} || 'no';
    $ENV{DEDUP_FD_CACHE}  = $DEDUP_FD_CACHE = $ENV{DEDUP_FD_CACHE} || 'no';
    $ENV{DEDUP_FS_OPTION} = $DEDUP_FS_OPTION = $ENV{DEDUP_FS_OPTION} || 'bothsides';
    $ENV{CLIENT}        = $CLIENT        = $ENV{CLIENT}        || "$HOST-fd";
    $ENV{LANG} = 'C';
    $out = ($debug) ? '@tee' : '@out';

    $TestName = $ENV{TestName} || basename($0);

    $dstat = $estat = $rstat = $bstat = $zstat = 0;
}

my $run_bconsole_silent=0;
# execute bconsole session
sub run_bconsole
{
    my $script = shift || "$tmp/bconcmds";
    my $silent="";
    if ($run_bconsole_silent) {
        $silent = "2> /dev/null 1> /dev/null";
    }
    return docmd("cat $script | $bin/bconsole -c $conf/bconsole.conf $silent");
}

# create a file-list for many tests using
# <$cwd/tmp/file-list as fileset
sub add_to_backup_list
{
    open(FP, ">$tmp/file-list") or die "ERROR: Unable to open $tmp/file-list $@";
    foreach my $l (@_) {
        if ($l =~ /\n$/) {
            print FP $l;
        } else {
            print FP $l, "\n";
        }
    }
    close(FP);
}

sub cleanup
{
    system("$rscripts/cleanup");
    return $? == 0;
}

sub start_test
{
    if ($FORCE_ALIGNED eq "yes") {
        if ($PREBUILT ne "yes") {
           system("make -C $cwd/build/src/plugins/sd install-aligned-plugin > /dev/null");
        }
        add_attribute("$conf/bacula-sd.conf", "Device Type", "Aligned", "Device");
        add_attribute("$conf/bacula-sd.conf", "Plugin Directory", "$plugins", "Storage");
    }
    if ($FORCE_CLOUD eq "yes") {
        add_attribute("$conf/bacula-sd.conf", "Device Type", "Cloud", "Device");
    }
    if ($FORCE_DEDUP eq "yes") {
        if ($PREBUILT ne "yes") {
           system("make -C $cwd/build/src/plugins/sd install-dedup-plugin > /dev/null");
        }
        add_attribute("$conf/bacula-sd.conf", "Device Type", "Dedup", "Device");
        add_attribute("$conf/bacula-sd.conf", "Plugin Directory", "$plugins", "Storage");
        add_attribute("$conf/bacula-sd.conf", "DedupDirectory", "${working}", "Storage");
        add_attribute("$conf/bacula-dir.conf", "Dedup", "$DEDUP_FS_OPTION", "Options");
        if ($DEDUP_FD_CACHE eq "yes") {
            add_attribute("$conf/bacula-fd.conf", "DedupIndexDirectory", "$working", "FileDaemon");
        }
    }

    $start_time = time();
    my $d = strftime('%R:%S', localtime($start_time));
    print "\n\n === Starting $TestName at $d ===\n";
}

sub end_test
{
    $end_time = time();
    my $t = strftime('%R:%S', localtime($end_time));
    my $d = strftime('%H:%M:%S', gmtime($end_time - $start_time));

    if ( -f "$tmp/err.log") {
        system("cat $tmp/err.log");
    }

    if ($estat != 0 || $zstat != 0 || $dstat != 0 || $bstat != 0 ) {
        print "
       !!!!! $TestName failed!!! $t $d !!!!!
         Status: estat=$estat zombie=$zstat backup=$bstat restore=$rstat diff=$dstat\n";

        if ($bstat != 0 || $rstat != 0) {
            print "     !!! Bad termination status       !!!\n";
        } else {
            print "     !!! Restored files differ        !!!\n";
        }
        print "     Status: backup=$bstat restore=$rstat diff=$dstat\n";
        print "     Test owner of $ENV{SITE_NAME} is $ENV{EMAIL}\n";
    } else {
        print "\n\n    === Ending $TestName at $t ($d) ===\n\n";
    }
}

# create a console command file, can handle a list
sub create_bconcmds
{
    open(FP, ">$tmp/bconcmds");
    map { print FP "$_\n"; } @_;
    close(FP);
}

# run a command
sub docmd
{
    my $cmd = shift;
    system("sh -c '$cmd " . (($debug)?"":" >/dev/null") . "'");
    return $? == 0;
}

sub start_bacula
{
    my $ret;
    $ret = docmd("$bin/bacula start");

    # cleanup bweb stuff
    create_bconcmds('@out /dev/null',
                    'sql',
                    'truncate client_group;',
                    'truncate client_group_member;',
                    'update Media set LocationId=0;',
                    'truncate location;',
                    '');
    run_bconsole();
    return $ret;
}

sub stop_bacula
{
    return docmd("$bin/bacula stop");
}

sub get_dirname
{
    my $ret = `$bin/bdirjson -c $conf/bacula-dir.conf -l Name -r Director`;
    if ($ret =~ /"Name": "(.+?)"/) {
        print "$1\n";
    }
}

sub get_resource
{
    my ($file, $type, $name) = @_;
    my $ret;
    open(FP, $file) or die "Can't open $file";
    my $content = join("", <FP>);

    if ($type eq 'FileSet') {
        if ($content =~ m/(^$type \{[^}]+?Name\s*=\s*"?$name"?.+?^\})/ms) {
            $ret = $1;
        }
    } else {
        if ($content =~ m/(^$type \{[^}]+?Name\s*=\s*"?$name"?[^}]+?^\})/ms) {
            $ret = $1;
        }
    }

    close(FP);
    return $ret;
}

sub get_attribute
{
    my ($file, $type, $name, $att) = @_;
    my $ret;
    open(FP, $file) or die "Can't open $file";
    my $content = join("", <FP>);

    if ($type eq 'FileSet') {
        if ($content =~ m/^$type \{[^}]+?Name\s*=\s*"?$name"?.+?$att\s*=\s*"?([^"^\s]+)"?[^}]+?^\}/ms) {
            $ret = $1;
        }
    } else {
        if ($content =~ m/^$type \{[^}]+?Name\s*=\s*"?$name"?[^}]+?$att\s*=\s*"?([^"^\s]+)"?[^}]+?^\}/ms) {
            $ret = $1;
        }
    }

    close(FP);
    if ($ret) {
        print $ret, "\n";
    }
}

sub extract_resource
{
    my $ret = get_resource(@_);
    if ($ret) {
        print $ret, "\n";
    }
}

sub get_field_size
{
    my ($file, $field) = @_;
    my $size=0;

    my $pattern=$field."\\s*([\\d,]+)";
    open(FP, $file) or die "ERROR: Can't open $file";
    
    while (<FP>) {
        if (/$pattern/) { 
            $size=$1;
        }
    }

    close(FP);

    $size =~ s/,//g;

    print $size."\n";
}

sub get_field_ratio
{
    my ($file, $field) = @_;
    my $ret=0;
    my $ratio=0;

    my $pattern=$field."\\s*[\\d.]+%\\s+([\\d]+)\.[\\d]*:1"; # stop at the '.'
    my $pattern2=$field."\\s*None";
    open(FP, $file) or die "ERROR: Can't open $file";
    
    while (<FP>) {
        if (/$pattern/) { 
            $ratio=$1;
        }
        if (/$pattern2/) { 
            $ratio="None";
        }
    }

    close(FP);

    $ratio =~ s/,//g;

    print $ratio."\n";
}

# Check the vacuum trace file for badrefs, automatically discard jobs in error
# in the catalog
sub check_vacuum_badref
{
    #           SELECT SessionId, SessionTime FROM Job WHERE JobStatus <> 'T'
    my ($trace, $errors) = @_;
    my %jobs;
    open(FP, $errors) or die "ERROR: Can't open $errors $!";
    while (my $line = <FP>) {
        $line =~ s/,//g;
        if ($line =~ /^\s*\|\s*(\d+)\s*\|\s*(\d+)/) {
            $jobs{"$1:$2"} = 1;
        }
    }
    close(FP);

    open(FP, $trace) or die "ERROR: Can't open $trace $!";
    while (my $line = <FP>) {
        if ($line =~ /VacuumBadRef OrphanRef FI=\d+ SessId=(\d+) SessTime=(\d+)/) {
            if (!$jobs{"$1:$2"}) {
                $estat=1;
                print $line;
            }
        }
    }
    close(FP);
}

sub check_max_backup_size
{
    my ($file, $size) = @_;
    my $ret=0;
    my $s=0;

    open(FP, $file) or die "ERROR: Can't open $file $!";
    
    while (<FP>) {

        if (/FD Bytes Written: +([\d,]+)/) { 
            $s=$1;
        }
    }

    close(FP);

    $size =~ s/,//g;

    if ($s > $size) { 
        print "ERROR: backup too big ($s > $size)\n";  
        $ret++;
    } else {
        print "OK\n";
    }
    return $ret;
}

sub check_min_volume_size
{
    my ($size, @vol) = @_;
    my $ret=0;

    foreach my $v (@vol) {
        if (! -f "$tmp/$v") {
            print "ERR: $tmp/$v not accessible\n";
            $ret++;
            next;
        }
        if (-s "$tmp/$v" < $size) {
            print "ERR: $tmp/$v too small\n";
            $ret++;
        }
    }
    $estat+=$ret;
    return $ret;
}

# check_volumes("tmp/log1.out", "tmp/log2.out", ...)
sub check_volumes
{
    my @files = @_;
    my %done;
    unlink("$tmp/check_volumes.out");
    unlink("$tmp/check_volumes_data.out");

    foreach my $f (@files) {
        open(FP, $f) or next;
        while (my $f = <FP>)
        {
            if ($f =~ /Wrote label to prelabeled Volume "(.+?)" on (?:dedup data|file) device "(.+?)" \((.+?)\)/) {
                if (!$done{$1}) {
                    $done{$1} = 1;
                    if (-f "$3/$1") {
                        system("$bin/bls -c $conf/bacula-sd.conf -j -E -V \"$1\" \"$2\" &>> $tmp/check_volumes.out");
                        if ($? != 0) {
                            debug("Found problems for $1, traces are in $tmp/check_volumes.out");
                            $estat = 1;
                        }
                        system("$bin/bextract -t -c $conf/bacula-sd.conf -V \"$1\" \"$2\" /tmp &>> $tmp/check_volumes_data.out");
                        if ($? != 0) {
                            debug("Found problems for $1, traces are in $tmp/check_volumes_data.out");
                            $estat = 1;
                        }
                    }
                }
            }
        }
        close(FP);
    }
    return $estat;
}

# Here we want to list all cloud parts and check what we have in the catalog
sub check_parts
{
    my $tempfile = "$tmp/check_parts.$$";
    open(FP, "|$bin/bconsole -c $conf/bconsole.conf >$tempfile");
    print FP "\@echo File generated by scripts::function::check_part()\n";
    print FP "sql\n";
    print FP "SELECT 'Name', VolumeName, Storage.Name FROM Media JOIN Storage USING (StorageId) WHERE VolType = 14;\n";
    close(FP);
    $run_bconsole_silent = 1;
    open(FP, $tempfile);
    while (my $l = <FP>) {
        $l =~ s/,//g;           # Default bacula output is putting , every 1000
        $l =~ s/\|/!/g;         # | is a special char in regexp
        if ($l =~ /!\s*Name\s*!\s*([\w\d-]+)\s*!\s*([\w\d-]+)\s*/) {

            unlink("$tmp/bconsole.cmd");
            open(CMD, ">$tmp/bconsole.cmd");
            print CMD "\@output $tmp/check_parts.out\n";
            print CMD "cloud list volume=$1 storage=$2\n";
            close(CMD);
            unlink("$tmp/check_parts.out");
            run_bconsole("$tmp/bconsole.cmd");
            open(OUT, "$tmp/check_parts.out");
            my @parts = ();
            while (my $l = <OUT>) {
                if ($l =~ /Error/) {
                    $estat=1;
                }
                if ($l =~ /\|\s*([\d]+)\s*\|\s*[\w\d\s\.]+\s*\|\s*[\w\d\s\-\:]+\s*\|/) {
                    push(@parts, $1);
                }
            }
            my $previous = 0;
            foreach (sort { $a <=> $b } @parts) {
                if ($previous != $_-1) {
                    print "ERR: volume $1. Dicontinuity in parts part.$previous-part.$_\n";
                    $estat=1;
                }
                $previous = $_;
            }
            close(OUT);
        }
    }
    close(FP);
}

# This test is supposed to detect JobMedia corruption for all jobs
# stored in the catalog.
sub check_jobmedia
{
    use bigint;

    my %jobids;
    my $ret=0;
    my %jobs;
    #  SELECT JobId, Min(FirstIndex) AS A FROM JobMedia GROUP BY JobId HAVING Min(FirstIndex) > 1;
    open(FP, "|$bin/bconsole -c $conf/bconsole.conf >$tmp/check_jobmedia.$$");
    print FP "\@echo File generated by scripts::function::check_jobmedia()\n";
    print FP "sql\n";
    if ($TestName !~ /(restart|incomplete)-/) {
        print FP "SELECT 'ERROR with FirstIndex not starting at 1 (JobId|FirstIndex)', JobId, Min(FirstIndex) AS A FROM JobMedia GROUP BY JobId HAVING Min(FirstIndex) > 1;\n";
    }
    print FP "SELECT 'ERROR with FirstIndex greater than LastIndex', JobId, JobMediaId, FirstIndex, LastIndex FROM JobMedia WHERE FirstIndex > LastIndex;\n";
    print FP "SELECT 'ERROR with LastIndex != JobFiles (JobId|LastIndex|JobFiles)', JobId, Max(LastIndex), JobFiles FROM Job JOIN JobMedia USING (JobId) WHERE JobStatus = 'T' AND Type = 'B' GROUP BY JobId,JobFiles HAVING Max(LastIndex) != JobFiles;\n";
    print FP "SELECT 'Index', JobId, FirstIndex, LastIndex, JobMediaId FROM JobMedia ORDER BY JobId, JobMediaId;\n";
    print FP "SELECT 'Block', JobId, MediaId, StartFile, EndFile, StartBlock, EndBlock, JobMediaId FROM JobMedia ORDER BY JobId, JobMediaId;\n";
    print FP "SELECT 'ERROR StartAddress > EndAddress (JobMediaId)', JobMediaId  from JobMedia where ((CAST(StartFile AS bigint)<<32) + StartBlock) > ((CAST (EndFile AS bigint) <<32) + EndBlock);\n";
    close(FP);

    my $tempfile = "$tmp/check_jobmedia.$$";
    open(FP, $tempfile);
    while (my $l = <FP>) {
        $l =~ s/,//g;           # Default bacula output is putting , every 1000
        $l =~ s/\|/!/g;         # | is a special char in regexp

        if ($l =~ /ERROR with LastIndex [\D]+(\d+)/) {
            print $l;
            print "HINT: Some FileIndex are not covered by a JobMedia. It usually means that you ",
                    "can't restore jobs impacted (jobid $1)\n\n";
            $jobids{$1}=1;
            $ret++;

        } elsif ($l =~ / ERROR /) {
            print $l;
            $ret++;
                     #              JobId     FirstIndex   LastIndex
                     #   Index  !     1     !         1 !      2277 !
        } elsif ($l =~ /Index\s*!\s*(\d+)\s*!\s*(\d+)\s*!\s*(\d+)\s*!/) {
            my ($jobid, $first, $last) = ($1, $2, $3);

            # incomplete tests are creating gaps in the FileIndex, no need to report these errors
            next if ($TestName =~ /(restart|incomplete|resume)[0-9]?-/);

            # Skip dummy records
            next if ($first == 0 && $last == 0);

            if ($jobs{$jobid} && !($jobs{$jobid} == $first || $jobs{$jobid} == ($first - 1))) {
                print "ERROR: found a gap in JobMedia, the FirstIndex is not equal to the previous LastIndex for jobid $jobid FirstIndex $first LastIndex $last PreviousLast $jobs{$jobid}\n";
                $ret++;
            }
            $jobs{$jobid} = $last;

                      #              JobId    MediaId     StartFile    EndFile   StartBlock  EndBlock     JobMediaId
                      # Block   !     2     !         3 !   1       !    1     !    129223 ! 999807168 !          4 !
        } elsif ($l =~ /Block\s*!\s*(\d+)\s*!\s*(\d+)\s*!\s*(\d+)\s*!\s*(\d+)\s*!\s*(\d+)\s*!\s*(\d+)\s*!/) {
            my ($jobid, $mediaid, $firstfile, $lastfile, $firstblk, $lastblk) = ($1, $2, $3, $4, $5, $6);

            my $first = ($firstfile << 32) + $firstblk;
            my $last = ($lastfile << 32) + $lastblk;

            if ($jobs{"$jobid:$mediaid"} && $jobs{"$jobid:$mediaid"} > $first) {
                print "ERROR: in JobMedia, previous Block is before the current Block for jobid=$jobid mediaid=$mediaid (";
                print $jobs{"$jobid:$mediaid"},  " > $first)\n";
                $ret++;
            }
            if ($last < $first) {
                print "ERROR: in JobMedia, the EndAddress is lower than the FirstAddress for JobId=$jobid MediaId=$mediaid ($last < $first)\n";
                $ret++;
            }
            $jobs{"$jobid:$mediaid"} = $last;
        }
    }
    close(FP);
    if ($ret) {
        print "ERROR: Found errors while checking JobMedia records, look the file $tempfile\n";
        if (scalar(%jobids)) {
            print "       The JobId list to check is dumped to $tmp/bad-jobid.out\n";
            open(FP, ">$tmp/bad-jobid.out");
            print FP join("\n", keys %jobids), "\n";
            close(FP);
        }
    }
    exit $ret;
}

# check if a volume is too big
# check_max_backup_size(10000, "vol1", "vol3");
sub check_max_volume_size
{
    my ($size, @vol) = @_;
    my $ret=0;

    foreach my $v (@vol) {
        if (! -f "$tmp/$v") {
            print "ERR: $tmp/$v not accessible\n";
            $ret++;
            next;
        }
        if (-s "$tmp/$v" > $size) {
            print "ERR: $tmp/$v too big\n";
            $ret++;
        }
    }
    $estat+=$ret;
    return $ret;
}

# update client definition for the current test
# it permits to test remote client
sub update_client
{
    my ($new_passwd, $new_address, $new_port) = @_;
    my $in_client=0;

    open(FP, "$conf/bacula-dir.conf") or die "can't open source $!";
    open(NEW, ">$tmp/bacula-dir.conf.$$") or die "can't open dest $!";
    while (my $l = <FP>) {
        if (!$in_client && $l =~ /^Client \{/) {
            $in_client=1;
        }
        
        if ($in_client && $l =~ /Address/i) {
            $l = "Address = $new_address\n";
        }

        if ($in_client && $l =~ /FDPort/i) {
            $l = "FDPort = $new_port\n";
        }

        if ($in_client && $l =~ /Password/i) {
            $l = "Password = \"$new_passwd\"\n";
        }

        if ($in_client && $l =~ /^\}/) {
            $in_client=0;
        }
        print NEW $l;
    }
    close(FP);
    close(NEW);
    my $ret = copy("$tmp/bacula-dir.conf.$$", "$conf/bacula-dir.conf");
    unlink("$tmp/bacula-dir.conf.$$");
    return $ret;
}

# if you want to run this function more than 100 times, please, update this number
my $last_update = 100;

# open a directory and update all files
sub update_some_files_rep
{
    my ($dest, $nbupdate)=@_;
    my $t=rand();
    my $f;
    my $nb=0;
    my $nbdel=0;
    my $total=0;

    if ($nbupdate) {
        $last_update = $nbupdate;
        unlink("$tmp/last_update");

    } elsif (-f "$tmp/last_update") {
        $last_update = `cat $tmp/last_update`;
        chomp($last_update);
        $last_update--;
        if ($last_update == 0) {
            $last_update = 100;
        }
    }
    my $base = chr($last_update % 26 + 65); # We use a base directory A-Z

    system("sh -c 'echo $last_update > $tmp/last_update'");
    print "Update files in $dest\n";
    opendir(DIR, "$dest/$base") || die "$!";
    map {
        $f = "$dest/$base/$_";
        if (($total++ % $last_update) == 0) {
            if (-f $f) {
                # We delete some of them, and we replace them later
                if ((($nb + $nbdel) % 11) == 0) {
                    unlink($f);
                    $nbdel++;

                    open(FP, ">$dest/$base/$last_update-$nbdel.txt") or die "$f $!";
                    seek(FP, $last_update * 4000, 0);
                    print FP "$t update $f\n";
                    close(FP);

                } else {
                    open(FP, ">>$f") or die "$f $!";
                    print FP "$t update $f\n";
                    close(FP);
                    $nb++;
                }
            }
        }
    } sort readdir(DIR);
    closedir DIR;
    print "$nb files updated, $nbdel deleted/created\n";
}

# open a directory and update all files
sub update_some_files
{
    my ($dest)=@_;
    my $t=rand();
    my $f;
    my $nb=0;
    print "Update files in $dest\n";
    opendir(DIR, $dest) || die "$!";
    map {
        $f = "$dest/$_";
        if (-f $f) {
            open(FP, ">$f") or die "$f $!";
            print FP "$t update $f\n";
            close(FP);
            $nb++;
        }
    } readdir(DIR);
    closedir DIR;
    print "$nb files updated\n";
}

# create big number of files in a given directory
# Inputs: dest  destination directory
#         nb    number of file to create
# Example:
# perl -Mscripts::functions -e 'create_many_files("$cwd/files", 100000)'
# perl -Mscripts::functions -e 'create_many_files("$cwd/files", 100000, 32000)'
sub create_many_files
{
    my ($dest, $nb, $sparse_size) = @_;
    my $base;
    my $dir=$dest;
    $nb = $nb / 2;              # We create 2 files per loop
    $nb = $nb || 750000;
    $sparse_size = $sparse_size || 0;
    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z

    # already done
    if (-f "$dest/$base/a${base}a${nb}aaa${base}") {
        debug("Files already created\n");
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create ", $nb * 2, " files into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        mkdir("$dest/$base") if (! -d "$dest/$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        open(FP, ">$dest/$base/a${base}a${i}aaa$base") or die "$dest/$base $!";
        print FP "$i\n";
        if ($sparse_size) {
            seek(FP, ($sparse_size + $i)/2, 1);
        }
        print FP "$i\n";
        if ($sparse_size) {
            seek(FP, ($sparse_size + $i)/2, 1);
        }
        print FP "$i\n";
        close(FP);
        
        open(FP, ">>$dir/b${base}a${i}csq$base") or die "$dir $!";
        print FP "$base $i\n";
        close(FP);
        
        if (!($i % 100)) {
            $dir = "$dest/$base/$base$i$base";
            mkdir $dir;
        }
        print "." if (!($i % 10000));
    }
    print "\n";
}

# create big number of files in a given directory
# Inputs: dest  destination directory
#         nb    number of file to create
# Example:
# perl -Mscripts::functions -e 'create_many_hardlinks("$cwd/files", 100000)'
# perl -Mscripts::functions -e 'create_many_hardlinks("$cwd/files", 100000, 32000)'
sub create_many_hardlinks
{
    my ($dest, $nb, $sparse_size) = @_;
    create_many_files($dest, $nb, $sparse_size);

    my $base;
    my $dir=$dest;
    $nb = $nb / 2;              # We create 2 files per loop
    $nb = $nb || 750000;

    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z

    # already done
    if (-f "$dest/$base$base/h-a${base}a${nb}aaa${base}") {
        debug("Files already created\n");
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create ", $nb * 2, " dirs into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        mkdir("$dest/$base$base") if (! -d "$dest/$base$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        link("$dest/$base/a${base}a${i}aaa$base", "$dest/$base$base/h-a${base}a${i}aaa$base") or die "$dest/$base $!";
        
        print "." if (!($i % 10000));
    }
    print "\n";
}

# BEEF
# create big number of files in a given directory
# Inputs: dest  destination directory
#         nb    number of file to create
# Example:
# perl -Mscripts::functions -e 'create_many_files_size("$cwd/files", 100000)'
sub create_many_files_size
{
    my ($dest, $nb) = @_;
    my $base;
    my $dir=$dest;
    $nb = $nb || 750000;
    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z

    # already done
    if (-f "$dest/$base/a${base}a${nb}aaa${base}") {
        debug("Files already created\n");
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create $nb files into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        mkdir("$dest/$base") if (! -d "$dest/$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        open(FP, ">$dest/$base/a${base}a${i}aaa$base") or die "$dest/$base $!";
        print FP "$base" x $i;
        close(FP);
        
        print "." if (!($i % 10000));
    }
    print "\n";
}

# create big number of dirs in a given directory
# Inputs: dest  destination directory
#         nb    number of dirs to create
# Example:
# perl -Mscripts::functions -e 'create_many_dirs("$cwd/files", 100000)'
sub create_many_dirs
{
    my ($dest, $nb) = @_;
    my ($base, $base2);
    my $dir=$dest;
    $nb = $nb || 750000;
    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z
    $base2 = chr(($nb+10) % 26 + 65);
    # already done
    if (-d "$dest/$base/$base2/$base/a${base}a${nb}aaa${base}") {
        debug("Files already created\n");
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create $nb dirs into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        $base2 = chr(($i+10) % 26 + 65);
        mkdir("$dest/$base");
        mkdir("$dest/$base/$base2");
        mkdir("$dest/$base/$base2/$base$base2");
        mkdir("$dest/$base/$base2/$base$base2/$base$base2");
        mkdir("$dest/$base/$base2/$base$base2/$base$base2/$base2$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        $base2 = chr(($i+10) % 26 + 65);
        mkdir("$dest/$base/$base2/$base$base2/$base$base2/$base2$base/a${base}a${i}aaa$base");  
        print "." if (!($i % 10000));
    }
    print "\n";
}

sub check_encoding
{
    if (grep {/Wanted SQL_ASCII, got UTF8/} 
        `${bin}/bacula-dir -d50 -t -c ${conf}/bacula-dir.conf 2>&1`)
    {
        print "Found database encoding problem, please modify the ",
              "database encoding (SQL_ASCII)\n";
        exit 1;
    }
}

sub set_global_maximum_concurrent_jobs
{
    my ($nb) = @_;
    add_attribute("$conf/bacula-dir.conf", "MaximumConcurrentJobs", $nb, "Job");
    add_attribute("$conf/bacula-dir.conf", "MaximumConcurrentJobs", $nb, "Client");
    add_attribute("$conf/bacula-dir.conf", "MaximumConcurrentJobs", $nb, "Director");
    add_attribute("$conf/bacula-dir.conf", "MaximumConcurrentJobs", $nb, "Storage");
    add_attribute("$conf/bacula-sd.conf", "MaximumConcurrentJobs", $nb+1, "Storage");
    add_attribute("$conf/bacula-sd.conf", "MaximumConcurrentJobs", $nb, "Device");
    add_attribute("$conf/bacula-fd.conf", "MaximumConcurrentJobs", $nb+1, "FileDaemon");
}

# You can change the maximum concurrent jobs for any config file
# If specified, you can change only one Resource or one type of
# resource at the time (optional)
#  set_maximum_concurrent_jobs('$conf/bacula-dir.conf', 100);
#  set_maximum_concurrent_jobs('$conf/bacula-dir.conf', 100, 'Director');
#  set_maximum_concurrent_jobs('$conf/bacula-dir.conf', 100, 'Device', 'Drive-0');
sub set_maximum_concurrent_jobs
{
    my ($file, $nb, $obj, $name) = @_;

    die "Can't get new maximumconcurrentjobs" 
        unless ($nb);

    add_attribute($file, "MaximumConcurrentJobs", $nb, $obj, $name);
}

# You can comment out a directive
#  comment_out('$conf/bacula-dir.conf', 'FDTimeout', 'Job', 'test');
#  comment_out('$conf/bacula-dir.conf', 'FDTimeout');
sub comment_out
{
    my ($file, $attr, $obj, $name) = @_;
    my ($cur_obj, $cur_name, $done);

    open(FP, ">$tmp/1.$$") or die "Can't write to $tmp/1.$$";
    open(SRC, $file) or die "Can't open $file";
    while (my $l = <SRC>)
    {
        if ($l =~ /^#/) {
            print FP $l;
            next;
        }

        if ($l =~ /^(\w+) \{/) {
            $cur_obj = $1;
            $done=0;
        }

        if ($l =~ /^\s*\Q$attr\E/i) {
            if (!$obj || $cur_obj eq $obj) {
                if (!$name || $cur_name eq $name) {
                    $l =~ s/^/##/;
                    $done=1
                }
            }
        }

        if ($l =~ /^\s*Name\s*=\s*"?([\w\d\.-]+)"?/i) {
            $cur_name = $1;
        }
        print FP $l;
    }
    close(SRC);
    close(FP);
    copy("$tmp/1.$$", $file) or die "Can't copy $tmp/1.$$ to $file";
}

# You can add option to a resource
#  add_attribute('$conf/bacula-dir.conf', 'FDTimeout', 1600, 'Director');
#  add_attribute('$conf/bacula-dir.conf', 'FDTimeout', 1600, 'Storage', 'FileStorage');
sub add_attribute
{
    my ($file, $attr, $value, $obj, $name) = @_;
    my ($cur_obj, $cur_name, $done);

    my $attr_ws=$attr;
    $attr_ws =~ s/\s+//g;
    
    my $is_options = $obj && $obj eq 'Options';
    if ($value =~ /\s/ && $value !~ m:[/"]:) { # exclude speed from the escape
        $value = "\"$value\"";
    }
    open(FP, ">$tmp/1.$$") or die "Can't write to $tmp/1.$$";
    open(SRC, $file) or die "Can't open $file";
    while (my $l = <SRC>)
    {
        if ($l =~ /^#/) {
            print FP $l;
            next;
        }

        if ($l =~ /^(\w+) \{/  || ($is_options && $l =~ /\s+(Options)\s*\{/)) {
            $cur_obj = $1;
            $done=0;
        }

        if ($l =~ /^\s*\Q$attr\E/i || $l =~ /^\s*\Q$attr_ws\E/i) {
            if (!$obj || $cur_obj eq $obj) {
                if (!$name || $cur_name eq $name) {
                    $l =~ s/\Q$attr\E\s*=\s*.+/$attr = $value/ig;
                    $done=1
                }
            }
        }

        if ($l =~ /^\s*Name\s*=\s*"?([\w\d\.-]+)"?/i) {
            $cur_name = $1;
        }

        my $add_missing = 0;
        if ($is_options) {
            if ($l =~ /\}/) {
                $add_missing = 1;
            }
        } elsif ($l =~ /^\}/) {
            $add_missing = 1;
        }
    
        if ($add_missing) {
            if (!$done) {
                if ($cur_obj && $cur_obj eq $obj) {
                    if (!$name || $cur_name eq $name) {
                        $l =~ s/\}/\n  $attr = $value\n\}/;
                    }
                }
            }
            $cur_name = $cur_obj = undef;
        }
        print FP $l;
    }
    close(SRC);
    close(FP);
    copy("$tmp/1.$$", $file) or die "Can't copy $tmp/1.$$ to $file";
}

# This test the list jobs output to check differences
# Input: read file argument
#        check if all jobids in argument are present in the first
#        'list jobs' and not present in the second
# Output: exit(1) if something goes wrong and print error
sub check_prune_list
{
    my $f = shift;
    my %to_check = map { $_ => 1} @_;
    my %seen;
    my $in_list_jobs=0;
    my $nb_list_job=0;
    my $nb_pruned=0;
    my $all=0;
    my $nb = scalar(@_);
    open(FP, $f) or die "Can't open $f $!";
    while (my $l = <FP>)          # read all files to check
    {
        if ($l =~ /list jobs/) {
            $in_list_jobs=1;
            $nb_list_job++;
            
            if ($nb_list_job == 2) {
                foreach my $jobid (keys %to_check) {
                    if (!$seen{$jobid}) {
                        print "ERROR: in $f, can't find JobId=$jobid in first 'list jobs'\n";
                        exit 1;
                    }
                }
            }
            next;
        }
        if ($nb_list_job == 0) {
            next;
        }
        if ($l =~ /prune (jobs|files) all/) {
            $all=1;
        }
        if ($l =~ /Pruned (\d+) Jobs? for client/i) {
            $nb_pruned += $1;
            if (!$all && $1 != $nb) {
                print "ERROR: in $f, Prune command returns $1 jobs, want $nb\n";
                exit 1;
            }
        }

        if ($l =~ /No Jobs found to prune/) {
           if (!$all && $nb != 0) {
                print "ERROR: in $f, Prune command returns 0 job, want $nb\n";
                exit 1;
            }            
        }

        # list jobs ouput:
        # | 1 | NightlySave | 2010-06-16 22:43:05 | B | F | 27 | 4173577 | T |
        if ($l =~ /^\|\s+(\d+)/) {
            if ($nb_list_job == 1) {
                $seen{$1}=1;
            } else {
                delete $seen{$1};
            }
        }
    }
    close(FP);
    if ($all && $nb_pruned != $nb) {
        print "ERROR: in $f, Prune command returns $nb_pruned job, want $nb\n";
        exit 1;
    }
    foreach my $jobid (keys %to_check) {
        if (!$seen{$jobid}) {
            print "******** listing of $f *********\n";
            system("cat $f");
            print "******** end listing of $f *********\n";
            print "ERROR: in $f, JobId=$jobid should not be, but is still present in the 2nd 'list jobs'\n";
            exit 1;
        }
    }
    if ($nb_list_job != 2) {
        print "ERROR: in $f, not enough 'list jobs'\n";
        exit 1;
    }
    exit 0;
}

# This test ensure that 'list copies' displays only each copy one time
#
# Input: read stream from stdin or with file list argument
#        check the number of copies with the ARGV[1]
# Output: exit(1) if something goes wrong and print error
sub check_multiple_copies
{
    my ($nb_to_found) = @_;

    my $in_list_copies=0;       # are we or not in a list copies block
    my $nb_found=0;             # count the number of copies found
    my $ret = 0;
    my %seen;

    while (my $l = <>)          # read all files to check
    {
        if ($l =~ /list copies/) {
            $in_list_copies=1;
            %seen = ();
            next;
        }

        # not in a list copies anymore
        if ($in_list_copies && $l =~ /^ /) {
            $in_list_copies=0;
            next;
        }

        # list copies ouput:
        # |     3 | Backup.2009-09-28 |  9 | DiskChangerMedia |
        if ($in_list_copies && $l =~ /^\|\s+\d+/) {
            my (undef, $jobid, undef, $copyid, undef) = split(/\s*\|\s*/, $l);
            if (exists $seen{$jobid}) {
                print "ERROR: $jobid/$copyid already known as $seen{$jobid}\n";
                $ret = 1;
            } else {
                $seen{$jobid}=$copyid;
                $nb_found++;
            }
        }
    }
    
    # test the number of copies against the given arg
    if ($nb_to_found && ($nb_to_found != $nb_found)) {
        print "ERROR: Found wrong number of copies ",
              "($nb_to_found != $nb_found)\n";
        exit 1;
    }

    exit $ret;
}

use POSIX qw/strftime/;
sub get_time
{
    my ($sec) = @_;
    print strftime('%F %T', localtime(time+$sec)), "\n";
}

sub debug
{
    if ($debug) {
        print join("\n", @_), "\n";
    }
}

sub p
{
    debug("\n################################################################",
          @_,
          "################################################################\n");
}

# check if binaries are OK
sub remote_check
{
    my $ret = 0;
    my $path = "/opt/bacula/bin";
    print "INFO: check binaries\n";
    foreach my $b (qw/bacula-fd bacula-dir bconsole bdirjson bsdjson
                      bfdjson bbconsjson bacula-sd/)
    {
        if (-x "$path/$b") {
            my $out = `$path/$b -? 2>&1`;
            if ($out !~ /Version:/g) {
                print "ERROR: with $b -?\n";
                system("$path/$b -?");
                $ret++;
            }
        }
    }
    foreach my $b (qw/bacula-sd/)
    {
        if (-r "$path/$b") {
            my $libs = `ldd $path/$b`;
            if ($libs !~ /tokyocabinet/g) {
                print "ERROR: unable to find tokyocabinet for $b\n";
                print $libs;
                $ret++;
            }
        }
    }

    return $ret;
}

sub remote_config
{
    open(FP, ">$REMOTE_FILE/bacula-fd.conf") or 
        die "ERROR: Can't open $REMOTE_FILE/bacula-fd.conf $!";

    my $plugins = '/opt/bacula/bin';
    if (-d '/opt/bacula/plugins') {
        $plugins = '/opt/bacula/plugins';
    }

    print FP "
Director {
  Name = $HOST-dir
  Password = \"$REMOTE_PASSWORD\"
}
FileDaemon {
  Name = remote-fd
  FDport = $REMOTE_PORT
  WorkingDirectory = $REMOTE_FILE/working
  Pid Directory = $REMOTE_FILE/working
  Plugin Directory = $plugins
  MaximumConcurrentJobs = 20
}
Messages {
  Name = Standard
  director = $HOST-dir = all, !skipped, !restored
}
";  
    close(FP);
    system("mkdir -p '$REMOTE_FILE/working' '$REMOTE_FILE/save'");
    system("rm -rf '$REMOTE_FILE/restore'");
    my $pid = fork();
    if (!$pid) {
        close(STDIN);  open(STDIN, "/dev/null");
        close(STDOUT); open(STDOUT, ">/dev/null");
        close(STDERR); open(STDERR, ">/dev/null");        
        exec("/opt/bacula/bin/bacula-fd -c $REMOTE_FILE/bacula-fd.conf");
        exit 1;
    }
    sleep(2);
    $pid = `cat $REMOTE_FILE/working/bacula-fd.$REMOTE_PORT.pid`;
    chomp($pid);

    # create files and tweak rights
    create_many_files("$REMOTE_FILE/save", 5000);
    chdir("$REMOTE_FILE/save");
    my $d = 'A';
    my $r = 0700;
    for my $g ( split(' ', $( )) {
        chmod $r++, $d;
        chown $<, $g, $d++;
    }
    
    # create a sparse file of 2MB
    init_delta("$REMOTE_FILE/save", 2000000);

    # create a simple script to execute
    open(FP, ">test.sh") or die "Can't open test.sh $!";
    print FP "#!/bin/sh\n";
    print FP "echo this is a script";
    close(FP);
    chmod 0755, "test.sh";

    # create a hardlink
    link("test.sh", "link-test.sh");

    # create long filename
    mkdir("b" x 255) or print "can't create long dir $!\n";
    copy("test.sh", ("b" x 255) . '/' . ("a" x 255)) or print "can't create long dir $!\n";

    # play with some symlinks
    symlink("test.sh", "sym-test.sh");
    symlink("$REMOTE_FILE/save/test.sh", "sym-abs-test.sh");

    if ($pid) {
        system("ps $pid");
        $estat = ($? != 0);
    } else {
        $estat = 1;
    }
}

sub remote_diff
{
    debug("Doing diff between save and restore");
    system("ssh $REMOTE_USER$REMOTE_ADDR " . 
     "$REMOTE_FILE/scripts/diff.pl -s $REMOTE_FILE/save -d $REMOTE_FILE/restore/$REMOTE_FILE/save");
    $dstat = ($? != 0);
}

sub remote_stop
{
    debug("Kill remote bacula-fd $REMOTE_ADDR");
    system("ssh $REMOTE_USER$REMOTE_ADDR " . 
             "'test -f $REMOTE_FILE/working/bacula-fd.$REMOTE_PORT.pid && " . 
              "kill `cat $REMOTE_FILE/working/bacula-fd.$REMOTE_PORT.pid`'");
}

sub remote_init
{
    system("ssh $REMOTE_USER$REMOTE_ADDR mkdir -p '$REMOTE_FILE/scripts/'");
    system("scp -q scripts/functions.pm scripts/diff.pl $REMOTE_USER$REMOTE_ADDR:$REMOTE_FILE/scripts/");
    system("scp -q config $REMOTE_USER$REMOTE_ADDR:$REMOTE_FILE/");
    debug("INFO: Configuring remote client");
    system("ssh $REMOTE_USER$REMOTE_ADDR 'cd $REMOTE_FILE && PERL5LIB=$REMOTE_FILE perl -Mscripts::functions -e remote_config'");
    system("ssh $REMOTE_USER$REMOTE_ADDR 'cd $REMOTE_FILE && PERL5LIB=$REMOTE_FILE perl -Mscripts::functions -e remote_check'");
}

sub get_mbytes_md5
{
    use Digest::MD5 qw/md5_hex/;
    my ($source, $cmd) = @_;
    my $buf;
    if (!open(FP1, $cmd)) {
        print "ERR\nCan't open $cmd $@\n";
        exit 1;
    }
    if (!open(FP, $source)) {
        print "ERR\nCan't open $source $@\n";
        exit 1;
    }
    while (my $l = <FP1>) {
        if ($l =~ /^(\d+):(\d+)/) {
            print "New chunk is $1:$2\n";
            seek(FP, $1, 0);
            my $nb=sysread(FP, $buf, $2);
            print $nb, ":", md5_hex($buf), "\n";
            print "WARNING: Unable to read $2, got $nb\n" if ($nb != $2);
        }
    }
    close(FP);
    close(FP1);
}

sub get_mbytes
{
    my ($source, $cmd, $binonly) = @_;
    my $buf;
    if (!open(FP1, $cmd)) {
        print "ERR\nCan't open $cmd $@\n";
        exit 1;
    }
    if (!open(FP, $source)) {
        print "ERR\nCan't open $source $@\n";
        exit 1;
    }
    while (my $l = <FP1>) {
        if ($l =~ /^(\d+):(\d+)/) {
            if (!$binonly) {
                print "New chunk is $1:$2\n";
            }
            seek(FP, $1, 0);
            sysread(FP, $buf, $2);
            print $buf;
            if (!$binonly) {
                print "\n";
            }
        }
    }
    close(FP);
    close(FP1);
}

sub get_bytes
{
    my ($file, $offset, $len) = @_;
    my $buf;
    if (!open(FP, $file)) {
        print "ERR\nCan't open $file $@\n";
        exit 1;
    }
    seek(FP, $offset, 0);
    sysread(FP, $buf, $len);
    print $buf, "\n";
    close(FP);
}

sub create_binfile
{
    my ($file, $nb) = @_;
    $nb ||= 10;

    if (!open(FP, ">$file")) {
        print "ERR\nCan't create txt $file $@\n";
        exit 1;
    }
    for (my $i = 0; $i < $nb ; $i++) {
        foreach my $c ('a'..'z') {
            my $l = ($c x 1024);
            print FP $l;
        }
    }
    close(FP);
}

my $c = "a";

sub init_delta
{
    my ($source, $sparse_size) = @_;

    $sparse_size = $sparse_size || 100000000;

    # Create $source if needed
    system("mkdir -p '$source'");

    if (!chdir($source)) {        
        print "ERR\nCan't access to $source $!\n";
        exit 1;
    }
 
    open(FP, ">text.txt") or return "ERR\nCan't create txt file $@\n";
    my $l = ($c x 80) . "\n";
    print FP $l x 40000;
    close(FP);

    open(FP, ">prev");
    print FP $c, "\n";
    close(FP);

    open(FP, ">sparse.dat") or return "ERR\nCan't create sparse $@\n";
    print FP $l x 40000;
    for (my $i = 0; $i < $sparse_size ; $i=$i+4096*1024) {
        seek(FP, $i, 0);
        print FP $l;
    }
    close(FP);
}

sub update_bytes
{
    my ($source, $offset, $len, $pattern) = @_;
    if (!$offset) {
        $offset = int((rand((-s $source) - 8192)) + 8192);
    }
    if (!$len) {
        $len = 512;
    }
    if (!$pattern) {
        $pattern = chr(rand(26) + ord('A'));
    }
    print "Updating $len bytes at offset $offset on $source with $pattern\n";
    open(FP, "+<", $source) or die "Unable to update $source. $@";
    seek(FP, $offset, 0);
    print FP $pattern x $len;
    close(FP);
}

sub update_delta
{
    my ($source) = shift;

    if (!chdir($source)) {        
        return "ERR\nCan't access to $source $!\n";
    }

    $c = `cat prev`;
    chomp($c);

    open(FP, "+<sparse.dat") or return "ERR\nCan't update the sparse file $@\n";
    for (my $i=0; $i < 5 ; $i++) {
        seek(FP, int(rand(-s "sparse.dat")), 0);
        print FP $c x 400;
        seek(FP, int(rand(-s "sparse.dat")), 0);
        print FP $c x 8000000;
    }
    seek(FP, 0, 2);
    print FP $c x 4000;
    close(FP);


    open(FP, ">>text.txt") or return "ERR\nCan't update txt file $@\n";    
    $c++;
    my $l = ($c x 80) . "\n";
    print FP $l x 40000;
    close(FP);

    open(FP, ">prev");
    print FP $c, "\n";
    close(FP);

    return "OK\n";
}

sub check_jobmedia_content
{
    use bigint;
    my ($jobmedia, $bls) = @_;
    my @lst;
    my $jm;

    open(FP, $jobmedia);

#  jobmediaid: 110
#       jobid: 10
#     mediaid: 2
#  volumename: Vol-0002
#  firstindex: 1
#   lastindex: 1
#   startfile: 0
#     endfile: 0
#  startblock: 903,387
#    endblock: 5,096,666

    while (my $line = <FP>) {
        if ($line =~ /(\w+): (.+)/) {
            my ($k, $t) = (lc($1), $2);
            $t =~ s/,//g;
            $jm->{$k} = $t;

            if ($k eq 'endblock') {
                $jm->{startaddress} = ($jm->{startfile} << 32) + $jm->{startblock};
                $jm->{endaddress} = ($jm->{endfile} << 32) + $jm->{endblock};
                push @lst, $jm;
                $jm = {};
            }
        }
    }
    close(FP);

    open(FP, $bls);
    #File:blk=0:11160794 blk_num=0 blen=64512 First rec FI=SOS_LABEL SessId=10 SessTim=1424160078 Strm=10 rlen=152
    my $volume;
    while (my $line = <FP>) {
        chomp($line);
        if ($line =~ /Ready to read from volume "(.+?)"/) {
            $volume = $1;
        }
        if ($line =~ /File:blk=(\d+):(\d+) blk_num=\d+ blen=(\d+)/) {
            my $found = 0;
            my ($address, $len) = (($1<<32) + $2, $3);
            foreach $jm (@lst) {
                if ($volume eq $jm->{volumename} && $address >= $jm->{startaddress} && $address <= $jm->{endaddress})
                {
                    $found = 1;
                    last;
                }
            }
            if (!$found) {
                print "ERROR: Address=$address len=$len volume=$volume not in BSR!!\n";
                print "$line\nJobMedia:\n";
                foreach $jm (@lst) {
                    if ($volume eq $jm->{volumename})
                    {
                        print "JobMediaId=$jm->{jobmediaid}\tStartAddress=$jm->{startaddress}\tEndAddress=$jm->{endaddress}\n";
                    }
                }
            }
        }
    }

    close(FP);
}

sub create_rconsole
{
    my ($name, $password, %acls) = @_;
    print "Console {
   Name = \"$name\"
   Password = \"$password\"\n";

    foreach my $acl (qw/Job Client Storage Pool Command Fileset Catalog Where RestoreClient BackupClient UserId Directory/) 
    {
        if (not exists $acls{$acl}) {
            print "   ${acl}ACL = *all*\n";
        } else {
            print "   ${acl}ACL = $acls{$acl}\n";
        }
    }
    print "}\n";
}

sub create_scratch_pool
{
    my ($file) = @_;
    if (get_resource($file, "Pool", "Scratch")) {
        return;
    }
    open(FP, ">>$file") or die "Can't write to $file";
    print FP "
Pool {
 Name = Scratch
 Recycle = yes
 AutoPrune = yes
 Pool Type = backup
 Recycle Pool = Scratch
}
";
    close(FP);
}

sub create_counter
{
    my ($file, $name, %params) = @_;
    if (get_resource($file, "Counter", $name)) {
        return;
    }
    $params{Minimum} = defined $params{Minimum} ? $params{Minimum} : 1;
    $params{Maximum} = defined $params{Maximum} ? $params{Maximum} : 9999;
    open(FP, ">>$file") or die "Can't write to $file";
    print FP "
Counter {
 Name = $name
 Minimum = $params{Minimum}
 Maximum = $params{Maximum}
 Catalog = MyCatalog
}
";
    close(FP);
}

sub _check_maxpoolbytes
{
    my ($pool, $delta, $ret) = @_;
    if ($pool->{name}) {
        if ($pool->{maxpoolbytes} > 0 && ($pool->{maxpoolbytes} + $delta) < $pool->{poolbytes}) {
            print "ERROR: Max Pool Bytes $pool->{name} $pool->{maxpoolbytes} < ($pool->{poolbytes} + $delta)\n";
            $ret = 1;
        } else {
            if ($debug) {
                print "OK: Max Pool Bytes $pool->{name} $pool->{maxpoolbytes} > ($pool->{poolbytes} + $delta)\n";
            }
            if ($pool->{maxpoolbytes} > 0 && $ret == 2) {
                $ret = 0;
            }
        }
    }
    return $ret;
}

sub check_maxpoolbytes_from_file
{
    my ($file, $pool) = @_;
    my %p;
    my $ret = 2;
    my $delta = 0;
    if ($FORCE_DEDUP eq 'yes') {
        $delta = 50000000;
    } elsif ($FORCE_ALIGNED eq 'yes') {
        $delta = 50000000;
    }
    open(FP, $file) or die "ERROR: Unable to open $tmp/out.$$";
    while (my $line = <FP>) {
        chomp($line);
        if ($line =~ /^\s*poolid:/i) {
            if (!$pool || ($p{name} && $p{name} eq $pool)) {
                $ret = _check_maxpoolbytes(\%p, $delta, $ret);
            }
            %p = ();
        }
        if ($line =~ /\s*(\w+):\s*(.+)/) {
            $p{lc($1)} = $2; 
        }
    }
    close(FP);
    $estat = _check_maxpoolbytes(\%p, $delta, $ret);

}

sub check_maxpoolbytes
{
    my ($pool) = @_;

    open(FP, ">$tmp/cmd.$$");
    print FP "\@output $tmp/out.$$\n";
    print FP "llist pool", ($pool?"=$pool":""), "\n";
    print FP "quit\n";
    close(FP);

    run_bconsole("$tmp/cmd.$$");
    check_maxpoolbytes_from_file("$tmp/out.$$", $pool);
}

sub check_json_tools
{
    my $test_json;
    eval 'use JSON qw/decode_json/; $test_json = JSON->new;';
    if ($@) {
        print "INFO: JSON validation is disabled ERR=$@\n";
    }
    my %progs = (
        "bdirjson" => "bacula-dir",
        "bsdjson"  => "bacula-sd",
        "bfdjson"  => "bacula-fd",
        "bbconsjson" => "bconsole",
    );
    for my $i (keys %progs) {
        my $json = `$bin/$i -c $conf/$progs{$i}.conf 2>&1`;
        if ($? != 0) {
            print "ERROR: $i exit code is $?\n";
            print "$json";
            $estat++;
        } else {
            if ($test_json) {
                eval {
                    my $var = $test_json->decode($json);
                };
                if ($@) {
                    print "ERROR: Unable to decode json data. ERR=$@\n";
                    #print "$json";
                    $estat++;
                }
            }
        }
    }
}

sub wait_for_async_requests
{
    my $driver = shift;
    my $count = 0;
    while ($count < 30) {
        if ($driver->get_eval('window.jQuery.active') == 0) {
            last;
        }
        $count++;
        sleep(1);
    }
}

use Fcntl 'SEEK_SET';
use Data::Dumper;
sub check_aligned_data
{
    my ($volume, $sourcefile) = @_;
    my $aligned = 512; #4096;
    my $pos = 0;
    my $line;
    my $linev;
    my $offset=0;
    my $len = 32;
    my $l;
    my $l2;

    open(SRC, $sourcefile) or die "ERROR: Unable to open $sourcefile $!";
    binmode SRC;

    open(VOL, $volume) or die "ERROR: Unable to open $volume $!";
    binmode VOL;

    # read the first line
    sysread(SRC, $line, $len);
    while (sysseek(VOL, $offset, SEEK_SET) && sysread(VOL, $linev, $len) == $len) {
        if ($line eq $linev) {
            # we found the start of the data
            while (($l = sysread(SRC, $line, $len)) > 0) {
                if (($l2 = sysread(VOL, $linev, $l)) != $l) {
                    print "ERROR: Found a difference between the source and the volume $l2 <> $l\n";
                    $estat=1;
                    return;
                }
                if ($line ne $linev) {
                    print "ERROR: Found a difference between the source and the volume at $offset\norg=";
                    foreach(split(//, $line)){
		       printf("0x%02x ",ord($_));
                    }
                    print " len=", length($line), "\nvol=";
                    foreach(split(//, $linev)){
		       printf("0x%02x ",ord($_));
                    }
                    print " len=", length($linev), "\n";
                    $estat=1;
                    return;
                }
                $offset += $l;
            }
            print "OK\n";
            return;
        }
        $offset += $aligned;
    }

    # check every X bytes if we found our first line

    close(SRC);
    close(VOL);
    print "ERROR: file '$sourcefile' not found inside $volume\n";
    $estat=1;
}

sub check_aligned_volumes
{
    my ($volbytes_max, $volabytes_min) = @_;
    my @vols;
    my %p;
    if ($FORCE_ALIGNED ne "yes") {
        return;
    }

    open(FP, ">$tmp/cmd.$$");
    print FP "\@output $tmp/out.$$\n";
    print FP "llist volume\n";
    print FP "quit\n";
    close(FP);
    run_bconsole("$tmp/cmd.$$");
    open(FP, "$tmp/out.$$") or die "ERROR: Unable to open $tmp/out.$$";
    while (my $line = <FP>) {
        chomp($line);
        if ($line =~ /^\s*volumename:/i) {
            if ($p{volumename}) {
                push @vols, \%p;
            }
            %p = ();
        }
        if ($line =~ /^\s*(\w+):\s*(.+)/) {
            my ($k, $v) = ($1, $2);
            $v =~ s/,//g;       # remove , from numbers
            $p{lc($k)} = $v;
        }
    }
    if ($p{volumename}) {
        push @vols, \%p;
    }
    close(FP);
    foreach my $vol (@vols) {
        if ($vol->{volbytes} > $volbytes_max) {
            print "ERROR: $vol->{volumename} volbytes $vol->{volbytes} > $volbytes_max\n";
            $estat++;
        }
        if ($vol->{volabytes} < $volabytes_min) {
            print "ERROR: $vol->{volumename} volabytes $vol->{volabytes} < $volabytes_min\n";
            $estat++;
        }
    }
}

sub check_encryption
{
    my ($log) = @_;
    my $found=0;
    my $job=0;
    open(FP, $log) or die "ERROR: Unable to open $log $@";
    while (my $line = <FP>) {
        if ($line =~ /JobId:\s(\d+)/) {
            $job=$1;
        }
        if ($line =~ /Encryption:\s*(yes|no)/) {
            if ($1 eq "yes") {
                $found=1;
            } else {
                print "ERROR: Found job $job with Encryption off in $log\n";
                $estat=1;
            }
        }
    }
    close(FP);
    if (!$found) {
        print "ERROR: No information about Encryption in $log\n";
        $estat=1;
    }
}

sub setup_fd_encryption
{
    my %opt = (
        signatures => 'yes',
        cipher => 'aes256',
        digest => 'sha256',
        keypair => "$conf/cryptokeypair.pem",
        masterkey => "$conf/master2048.cert",
        fdconf => "$conf/bacula-fd.conf",
        encryption => 'yes');

    %opt = (@_, %opt);

    my $fdconf = $opt{fdconf};
    delete $opt{fdconf};
    
    for my $i (keys %opt) {
        add_attribute($fdconf, "PKI $i", "\"$opt{$i}\"", "FileDaemon");
    }

    if (! -f $opt{keypair}) {
        copy("$rscripts/cryptokeypair.pem", $opt{keypair});
    }
    if (! -f $opt{masterkey}) {
        copy("$rscripts/master2048.cert", $opt{masterkey});
    }
}

sub setup_collector
{
    my ($conf, $name, $type, $interval) = @_;
    my $file='';

    $name = $name || "collector1";
    $type = $type || "csv";
    $interval = $interval || 60;

    if ($type eq 'csv') {
        $file = "File = \"$tmp/$name.csv\"";
    } else {
        $file = "Host = localhost\nPort = 2003\n";
    }
    open(FP, ">>$conf") or die "Error: Unable to open $conf $@";
    print FP "
Statistics {
  Name = $name
  Interval = 60
  Type = $type
  $file
}
";
    close(FP);
}

use IO::Socket::INET;

sub check_tcp
{
    my ($host, $port) = @_;
    my $sock = IO::Socket::INET->new(PeerAddr => $host,
                                     PeerPort => $port,
                                     Proto    => 'tcp') or die "Error: check_tcp Unable to connect $host:$port $@";
    $sock->write("Hello !\n");
    $sock->close();
}

sub check_tcp_loop
{
    my ($pid, $host, $port) = @_;
    my $count=5;
    while (! -f $pid && $count > 0) {
        if ($debug) {
            print "Waiting for $pid to appear\n";
        }
        $count--;
        sleep(1);
    }
    $count=0;
    while (-f $pid) {
        check_tcp($host, $port);
        $count++;
        sleep(1);
    }
    if ($count > 0) {
        open(FP, ">$tmp/$host.$port.probe") or die "ERROR: Unable to open $tmp/$host.$port.probe $@";
        print FP "$count\n";
        close(FP);
    }
    print "Did $count network probes on $host:$port\n";
    return $count;
}

sub setup_fdcallsdir
{
    my $out = `$bin/bfdjson -r director -l Name -D | grep Name | head -1`;
    $out =~ /"Name":\s*"(.+)"/;
    my $name = $1;
    # TODO: Need to drop this line
    add_attribute("$conf/bacula-dir.conf", "Address", "1.1.1.1", "Client");
    add_attribute("$conf/bacula-dir.conf", "AllowFDConnections", "yes", "Client");
    add_attribute("$conf/bacula-fd.conf", "ConnectToDirector", "yes", "Director", $name);
    add_attribute("$conf/bacula-fd.conf", "Address", "127.0.0.1", "Director", $name);
    add_attribute("$conf/bacula-fd.conf", "DirPort", "$BASEPORT", "Director", $name);
}

sub setup_tls
{
    my ($file, $res, $name) = @_;
    if (! -f "$conf/tls-cert.pem") {
        copy("$rscripts/tls-cert.pem", $conf);
        copy("$rscripts/tls-CA.pem", $conf);
    }
    add_attribute($file, "TLS Require", "yes", $res, $name);
    add_attribute($file, "TLS Certificate", "$conf/tls-cert.pem", $res, $name);
    add_attribute($file, "TLS Key", "$conf/tls-cert.pem", $res, $name);
    add_attribute($file, "TLS CA Certificate File", "$conf/tls-CA.pem", $res, $name);
}

sub setup_fd_tls
{
    my ($file) = @_;
    setup_tls($file, "Director");
    setup_tls($file, "FileDaemon");
    setup_tls($file, "Console");
}

sub setup_dir_tls
{
    my ($file) = @_;
    setup_tls($file, "Director");
    setup_tls($file, "Client");
    setup_tls($file, "Storage");
    setup_tls($file, "Autochanger");
    setup_tls($file, "Console");
}

sub setup_sd_tls
{
    my ($file) = @_;
    setup_tls($file, "Storage");
    setup_tls($file, "Director");
}

sub setup_cons_tls
{
    my ($file) = @_;
    setup_tls($file, "Director");
}

sub check_openfile
{
    my ($dir, $limit, $name) = @_;
    my $msg = "";

    $limit = $limit || 10;
    $name = $name || "Vol";

    if (!open(FP, "ls -l $dir|")) {
        print "Unable to open $dir $@\n";
        return 0;
    }
    my $count = 0;
    while (my $line = <FP>) {
        # lrwx------ 1 eric users 64 Jan 25 19:10 0 -> /dev/pts/9
        # lrwx------ 1 eric users 64 Jan 25 19:10 1 -> /dev/pts/9
        # lrwx------ 1 eric users 64 Jan 25 19:10 2 -> /dev/pts/9
        if ($line =~ m: (\d+)\s+->\s+(/.+):) {
            my ($fileno, $file) = ($1, $2);
            if ($file =~ m:/dde[0-9]?/:) {
                next;
            }
            $count++;
            if ($file =~ /$name/) {
                $estat=1;
                print "ERROR: Found volume still open at the end of the test $fileno -> $file\n";
            }
        }
    }
    close(FP);
    if ($count > $limit) {
        system("ls -l $dir");
        print "ERROR: Found more than $limit files unexpectedly open ($count)";
        $estat=1;
    }
    return $estat;
}

sub check_cloud_hash
{
    my (@files) = @_;
    my $error=0;
    my $jobs;
    my $vols;
    foreach my $file (@files) {
        open(FP, $file) or die "Unable to open $file";
        while (my $line = <FP>) {
            if ($line =~ /JobId (\d+): Cloud Download transfers:/) {
                $jobs->{$1} = 'R';
            }
            if ($line =~ /JobId (\d+): Cloud Upload transfers:/) {
                $jobs->{$1} = 'B';
            }
            # JobId 2: TestVolume001/part.2     state=done    size=451.5 KB duration=2s hash=078aa1778f54d814
            if ($line =~ /JobId (\d+): ([\w\d_-]+\/part.\d+)\s+state=(\w+)\s+size=.+? duration=.+? hash=([\w\d]+)/) {
                if ($3 ne 'done') {
                    print "ERROR: $2 state is $3\n";
                    $error++;
                }
                if ($jobs->{$1} eq 'R') {
                    if (!$vols->{$2}) {
                        print "ERROR: $2 not found in backup\n";
                        $error++;
                    } elsif ($vols->{$2} ne $4) {
                        print "ERROR: hash for $2 missmatch\n";
                        $error++;
                    }
                }
                $vols->{$2} = $4;
            }
        }
        close(FP);
    }
    exit $error;
}

sub add_log_message
{
    my (@files) = @_;
    foreach my $file (@files) {
        $file =~ m:/([^/]+)$: or die "ERROR: Unable to find the filename";
        my $trace=$1;
        open(FP, $file) or die "Unable to open $file";
        open(FP2, ">$tmp/1.$$") or die "Unable to open temporary file $tmp/1.$$";
        while (my $line = <FP>) {
            if ($line =~ /Messages \{/) {
                print FP2 $line;
                print FP2 "append = \"$working/$trace.log\" = all, !skipped\n";
            } else {
                print FP2 $line;
            }
        }
        close(FP2);
        close(FP);
        unlink($file);
        rename("$tmp/1.$$", $file);
    }
}

# Check a bscan -r output to see if we have potential issues
# (like mixed records, missing fileindex, ...)
sub check_bscan
{
    my ($file) = @_;
    my %job;
    my %lines;
    my $curvol="";
    my $nl=0;
    my $nls;
    open(FP, $file) or die "Cannot open $file. $@";
    while (my $line = <FP>)
    {
        $nl++;
        $nls = sprintf("%08d", $nl);
        chomp($line);
        # From this debug line, we can determine the volume name (in the cloud debug=10,cloud
        if ($line =~ /open mode=OPEN_READ_ONLY open\((.+?),/) {
            $curvol = $1;

        } elsif ($line =~ /Record: SessId=(\d+) SessTim=(\d+) FileIndex=(\d+) Stream=(\d+) len=(\d+)/) {
            my $j = "$1-$2";    # sessid-sesstime
            my $FI = $3;        # FileIndex
            next if ($FI <= 0); # Skip LABELs (can be also deleted files FIXME)

            # Backup not yet seen
            if (not exists $job{$j}) {
                # We start at more than 1, maybe the previous volume was not bscanned ?
                if ($FI > 1) {
                    $estat++;
                    print "Suspicious line-$nls: (first FI too high $FI)\n";
                    print "$line\n";
                }
            # We have a previous job record, but the Fileindex is not the one we expect
            } elsif ($job{$j} > 1 && $FI > 1 && ($job{$j} != $FI) && ($job{$j} + 1) != $FI) {
                $estat++;
                print "Suspicious line-$nls: FI-1=$job{$j} FI=$FI vol=$curvol\n";
                print "  $lines{$j}\n";
                print "  $nls $line $curvol\n";

            }
            $job{$j} = $FI;
            $lines{$j} = "$nls $line $curvol";
        }
    }
    close(FP);
}

# This function takes log files with job status, estimate, and list files
# output. Estimate must match what the backup is doing
sub compare_backup_content
{
    my ($joblog, $files, $estimate) = @_;
    open(FP, $files) or die "Unable to open $files $@";
    my %content;
    my $header=0;
    my $nb=0;
    while (my $line = <FP>) {
        if ($line =~ /\+------/) {
            $header++;
            next;
        }
        # +----------------------------------+
        # | filename                         |
        # +----------------------------------+
        # | c:/tmp/                          |
        # +----------------------------------+
        if ($header == 2) {
            if ($line =~ /\|\s+(.+?)\s*\|/) {
                $content{$1} = 1;
                #print "   Add [$1]\n";
            }
        }
    }
    close(FP);
    open(FP, $estimate) or die "Unable to open $estimate $@";
    while (my $line = <FP>) {
        last if ($nb > 10);
        chomp($line);
        #-rwxrwxrwx   1 0        0                        22 2019-12-19 00:53:13  c:/tmp/hello.txt
        my ($type, $nlinks, $u, $g, $s, $y, $h, $f) = split(/ +/, $line, 8);
        if (!$f) {
            next;               # Not the correct line
        }
        if ($type =~ /^d/ && $f !~ m:/$:) {
            $f = $f . "/";
        }
        #print "Checking: [$f]\n";
        if (exists $content{$f}) {
            $content{$f} = 0;
        } else {
            if ($f =~ m=C:/ProgramData/Microsoft/Search/Data/Applications/Windows=i) {
                # the files in thes directories looks to be reparse point and
                # don't show up in snapshot
                print "WARNING: [$f] not found in job log but ignored\n";
            } else {
                print "ERROR: [$f] not found in job log\n";
                $estat=1;
                $nb++;
            }
        }
    }
    $nb=0;
    foreach my $f (keys %content) {
        next if ($f =~ /pagefile.sys/); # Skip swap file, not always in estimate...
        last if ($nb > 10);
        if ($content{$f} == 1) {
            print "ERROR: [$f] not found in estimate\n";
            $estat=1;
            $nb++;
        }
    }
    close(FP);
}

#
# This function takes a trace file and checks if the TLS setup is properly done
#
sub check_tls_traces
{
    my ($tracefile, $type) = @_;
    my $localneed;
    my $remoteneed;
    my $finaltype;
    my $count=0;

    if (open(FP, $tracefile)) {
        while (my $line = <FP>) {
            if ($line =~ /TLSPSK Local need (\d+)/) {
                $localneed = $1;
            }
            if ($line =~ /TLSPSK Remote need (\d+)/) {
                $remoteneed = $1;
            }
            if ($line =~ /TLSPSK Start (\w+)/) {
                $count++;
                $finaltype = $1;
                if ($type && $type ne $finaltype) {
                    print "ERROR: Must find $type connection, found $finaltype in $tracefile\n";
                    $estat=1;
                }

                my $localtlsneed = $localneed / 100;
                my $localpskneed = $localneed % 100;
                my $remotetlsneed = $remoteneed / 100;
                my $remotepskneed = $remoteneed % 100;
                # None 0, OK 1, Require 2
            }
        }
        close(FP);
    }
    if ($count == 0) {
        print "ERROR: Must find at least one connection in $tracefile\n";
        $estat=1;
    }
}

#
# Check if we have basic events
#
sub check_events
{
    my $ret=1;
    open(FP, "|$bin/bconsole -c $conf/bconsole.conf >$tmp/check_events.$$");
    print FP "\@echo File generated by scripts::function::check_events()\n";
    print FP "list events\n";
    print FP "quit\n";
    close(FP);

    my $tempfile = "$tmp/check_events.$$";
    open(FP, $tempfile);
    while (my $l = <FP>) {
        $l =~ s/\|/!/g;         # | is a special char in regexp
        if ($l =~ /Director startup/) {
            $ret = 0;
        }
    }
    close(FP);
    if ($ret) {
        print "ERROR: Found errors while checking Event records, look the file $tmp/check_events.$$\n";
        $estat=1;
    }
}

sub check_events_json
{
    my ($name, $dest, $event) = @_;
    my $test_json;
    eval 'use JSON qw/decode_json/; $test_json = JSON->new;';
    if ($@) {
        print "ERROR: json test skipped\n";
        return;
    }

    system("$bin/bdirjson -c $conf/bacula-dir.conf -r messages -n '$name' > $tmp/json.1");
    if ($? != 0) {
        print "ERROR: bdirjson exit code is incorrect $?\n";
        $estat=1;
        return;
    }

    open(FP, "$tmp/json.1");
    my $lines = join("", <FP>);
    close(FP);
    my $obj = $test_json->decode($lines);
    if (!$obj) {
        print "ERROR: unable to parse json output\n";
        $estat=1;
        return;
    }

    #use Data::Dumper qw/Dumper/;
    #print Dumper($obj->[0]->{Messages}->{Destinations}->[0]->{MsgTypes});
    #print Dumper($obj);

    my $ok=0;
    foreach my $destinations (@{$obj->{Destinations}}) {
        next if ($destinations->{Type} ne $dest);
        if (grep(/^$event$/, @{$destinations->{MsgTypes}})) {
            $ok = 1;
        }
    }

    if (!$ok) {
        print "ERROR: event not found in Resource $name/$dest\n";
        $estat=1;
        return;
    }
}

# print + \n
sub println
{
    print "\n", @_, "\n";
}

sub add_virtual_changer
{
    my ($name, $nb_drives) = @_;
    if (!open(FP, ">>$conf/bacula-sd.conf")) {
        print "ERROR: Unable to open $conf/bacula-sd.conf $@\n";
        $estat=1;
        return;
    }

    $nb_drives--;               # Let's start at 0

    my $devices = join(",", map { "$name-Drive-$_" } 0..$nb_drives);
    print FP "
Autochanger {
  Name = $name
  Changer Device = /dev/null
  Changer Command = /dev/null
  Device = $devices
}
";

    for my $nb (0..$nb_drives) {
        print FP "
Device {
  Name = $name-Drive-$nb
  Device Type = File
  Media Type = ${name}Type
  Archive Device = $tmp
  AutomaticMount = yes;               # when device opened, read it
  Autochanger = yes
  Drive Index = $nb
  AlwaysOpen = yes;
  RemovableMedia = yes;
  Label Media = yes
}
";
    }
    close(FP);
    my $config = `$bin/bdirjson -c $conf/bacula-dir.conf -r storage`;
    if ($config !~ m/"Name": "([\w\d]+)"/) {
        print "ERROR: Unable to find an existing storage resource in $conf/bacula-dir.conf\n";
        $estat=1;
        return;
    }
    $config = get_resource("$conf/bacula-dir.conf", "Storage", $1);
    if (!$config) {
        $config = get_resource("$conf/bacula-dir.conf", "Autochanger", $1);
        $config =~ s/Autochanger/Storage/g;
    }
    open(FP, ">$tmp/1");
    print FP $config;
    close(FP);

    add_attribute("$tmp/1", "Name", $name, "Storage");
    add_attribute("$tmp/1", "Device", $name, "Storage");
    add_attribute("$tmp/1", "MaximumConcurrentJobs", $nb_drives+1, "Storage", $name);
    add_attribute("$tmp/1", "Media Type", "${name}Type", "Storage");
    add_attribute("$tmp/1", "LabelFornat", "Vol", "Pool",);
    system("cat $tmp/1 >> $conf/bacula-dir.conf");
}

sub check_dot_status
{
    my ($file, $command) = @_;

    $file ||= "$tmp/check_dot_status.ctrl";
    $command ||= "\@output $tmp/check_dot_status.out\nreload\n.api 2\n.status dir running\n";

    system("touch $file");
    open(FP, ">$tmp/check_dot_status.cmd");
    print FP $command;
    close(FP);
    while ( -f $file) {
        run_bconsole("$tmp/check_dot_status.cmd");
    }
}

sub parse_fuse_trace
{
    my ($file) = @_;
    open(FP, $file);
    while (my $line = <FP>) {
    # 02-Jul-2020 17:45:21 bacula-fused[5]: bfuse_daemon.c:1341-0 Call 2048 = read(/MyCatalog/test-fd/382/@vsphere/vm-547/0.bvmdk, size=2048, offset=24) crc=473e001 ctx=0x7f7714015e80
        if ($line =~ m:Call \d+ = read\(/.+?/(\d+).bvmdk, size=(\d+), offset=(\d+)\) :) {
            print "$3:$2\n";
        }
    }
    close(FP);
}

sub generate_random_seek
{
    use bigint;
    my ($file, $nb, $readsize) = @_;
    my $size = -s $file;
    $nb ||= 10000;
    $readsize ||= 0;
    for my $i (1..$nb) {
        my $sz=65635;
        my $pos = int(rand($size));
        if (($i % 5) == 0) {
            $pos = $pos / 512;
        }
        if (($i % 7) == 0) {
            $pos = $pos / 1024;
        }
        if ($readsize == 0) {
            $sz = int(rand($sz));
        }
        print "$pos:$sz\n";
    }
}

# We search for the @# "EXPECT: pattern" message and we compare the pattern
# with the next "Storage:" line. If we have a missmatch, we display a message
# and the status is in error.
sub check_storage_selection
{
    my ($file) = @_;
    my $error = 0;
    my $selection="not set";
    my $found = "";
    my $nb=0;
    my $nb_err=0;
    my $err=0;
    open(FP, $file) or die "ERROR: Unable to open $file $@";
    while (my $line = <FP>) {
        $nb++;
        if ($line =~ /EXPECT: (.+?)"/) {
            if ($error) {
                print "ERROR: Expecting \"$selection\", got \"$found\". from $file:$nb_err\n";
                $err++;
            }
            $error = 0;
            $selection = $1;
        }
        # We might found multiple lines with Storage, we take the last one
        if ($line =~ /Storage: .+?\((.+?)\)/) {
            $found = $1;
            if ($found ne $selection) {
                $error = 1;
                $nb_err = $nb;  # Keep the current line
            } else {
                $error = 0;
            }
        } else {
            #print $line;
        }
    }
    if ($error) {
        print "ERROR: Expecting \"$selection\", got \"$found\" from $file:$nb_err\n";
        $err++;
    }
    close(FP);
    exit $err;
}

# We validate a json string
sub check_json
{
    my ($file) = @_;
    open(FP, $file) or die "ERROR: Unable to open $file";
    my $content = join("", <FP>);    
    close(FP);

    my $test_json;
    eval 'use JSON qw/decode_json/; $test_json = JSON->new;';
    if ($@) {
        print "INFO: JSON validation is disabled ERR=$@\n";
        exit 1;
    }

    eval {
        my $var = $test_json->decode($content);
    };
    if ($@) {
        print "ERROR: Unable to decode json data. ERR=$@\n";
        exit 1;
    }
    exit 0;
}

# Get the permission in octal for a file, equivalent to stat -c %a
sub get_perm
{
    my ($file) = @_;
    my $mode = (stat($file))[2];
    printf "%04o\n", $mode & 07777;
}

# Parse the output of llist volume to check the protection parameters
sub check_protect
{
    my ($file, $name, $status, $protect, $useprotect) = @_;
    open(FP, $file) or die "Unable to open $file. $!";
    my $found=0;
    while (my $line = <FP>) {
        if ($line =~ /volumename: (\S+)/i) {
            if ($name eq $1) {
                $found=1;
            } else {
                $found=0;
            }
        }
        if ($found) {
            if ($line =~ /volstatus: +(\S+)/i) {
                if (defined $status && $status ne $1) {
                    print "ERROR: volstatus for $name $status != $1 in $file\n";
                    exit 1;
                }
            } elsif ($line =~ /useprotect: +(\S+)/i) {
                if (defined $useprotect && $useprotect != $1) {
                    print "ERROR: useprotect for $name $protect != $1 in $file\n";
                    exit 1;
                }
            } elsif ($line =~ /protect: +(\S+)/i) {
                if (defined $protect && $protect != $1) {
                    print "ERROR: protect for $name $protect != $1 in $file $line\n";
                    exit 1;
                }
            }
        }
    }
    close(FP);
}

sub get_running_jobs
{
    my ($client, $status) = @_;
    my $count = 0;
    my $tempfile = "$tmp/running_jobs.$$";
    eval 'use JSON qw/decode_json/;';
    if ($@) {
        print "INFO: JSON validation is disabled ERR=$@\n";
    }
    open(FP, "|$bin/bconsole -c $conf/bconsole.conf >$tempfile");
    print FP ".api 2 api_opts=j\n";
    print FP ".status dir running client=$client\n";
    close(FP);
    open(FP, $tempfile);
    <FP>; # Connecting to Director 127.0.0.1:8101
    <FP>; # 1000 OK: 10002 127.0.0.1-dir Version: 16.0.1 (03 February 2023)
    <FP>; # Enter a period to cancel a command.
    <FP>; # .api 2 api_opts=j
    <FP>; # .status dir running client=127.0.0.1-fd
    my $data = decode_json(<FP>);
    close(FP);
    if ($status) {
        $count = scalar(grep { $_->{status} eq $status } @{ $data->{running} });
    } else {
        $count = scalar( @{ $data->{running} });
    }
    print "$count\n";
}

sub get_client_name
{
    my $line = `$bin/bdirjson -r client -l Name -D`;
    my $test_json;
    eval 'use JSON qw/decode_json/; $test_json = JSON->new;';
    if ($@) {
        print "ERROR: json test skipped\n";
        return;
    }
    my $r = $test_json->decode($line);
    print "$r->[0]->{Name}\n";
}

1;
