#!/usr/bin/env python3
# -*- coding: utf-8 -*
# this program compare two branche of GIT
# and show the differences
from __future__ import print_function
import sys
import os
import logging
import collections
import re
import argparse
import time
import codecs
import difflib
import itertools

try:
    import git
except ImportError:
    print("you must install python-git aka GitPython", file=sys.stderr)
    sys.exit(1)


def add_console_logger():
    console=logging.StreamHandler()
    console.setFormatter(logging.Formatter('%(levelname)-3.3s %(filename)s:%(lineno)d %(message)s', '%H:%M:%S'))
    console.setLevel(logging.INFO) # must be INFO for prod
    logging.getLogger().addHandler(console)
    return console

def add_file_logger(filename):
    filelog=logging.FileHandler(filename)
    # %(asctime)s  '%Y-%m-%d %H:%M:%S'
    filelog.setFormatter(logging.Formatter('%(asctime)s %(levelname)-3.3s %(filename)s:%(lineno)d %(message)s', '%H:%M:%S'))
    filelog.setLevel(logging.INFO)
    logging.getLogger().addHandler(filelog)
    return filelog


def run_cmp_branch(args):
    repo=args.repo
    # check if the branch exists
    f1=f2=False
    sources=[]
    if args.branch1.startswith('re:'):
        try:
            branch_re=re.compile(args.branch1[3:])
        except re.error:
            print("invalid regex: ", args.branch1)
            return False
        for ref in repo.references:
            if branch_re.match(ref.name):
                sources.append(ref)
        if not sources:
            print("no match: ", args.branch1)
            return False
    else:
        sources.append(args.branch1)

    for ref in repo.references:
        f1|=(args.branch1.startswith('re:') or args.branch1==ref.name)
        f2|=(args.branch2==ref.name)
    for found, name in ((f1, args.branch1), (f2, args.branch2)):
        if not found:
            print("Branch not found: ", name)
            return False

    branch2_orig=args.branch2
    if args.path:
        # check if the paths are valid in the branch2
        for path in args.path:
            path_match=False
            for entry in repo.commit(branch2_orig).tree.traverse():
                if entry.path.startswith(path):
                    path_match=True
                    break
            if not path_match:
                args._parser.error("path not found in {}: {}".format(branch2_orig, path));

    for branch1 in sources:
        branch2=branch2_orig
        if args.switch:
            branch1, branch2=branch2, branch1
        if args.short_legend or args.branch1.startswith('re:'):
            print("=== Compare branch %(branch1)s and %(branch2)s" %
                  dict(branch1=branch1, branch2=branch2))
        elif not args.no_legend:
            print(cmp_branch_legend % dict(branch1=branch1, branch2=branch2))
#       for commit in repo.iter_commits(branch1, max_count=10):
#           print commit.hexsha, commit.committed_date, commit.author.name, commit.message

#       print dir(repo)
        commons=repo.merge_base(branch1, branch2)
        if len(commons)!=1:
            print("cannot find the unique common commit between", branch1, branch2)
            return False

        common=commons[0]
        # make a list of all know commit in branch-2
        commits2=set() # (authored_date, author_name, subject)
        commits2b=set() # (author_name, subject) to detect modified patch
        commits2m=dict()  # (authored_date, author_name) to detect modified message
        for commit in repo.iter_commits(branch2):
            if commit.hexsha==common.hexsha:
                break

            subject=commit.message.split('\n', 1)[0]
            commits2.add((commit.authored_date, commit.author.name, subject))
            commits2b.add((commit.author.name, subject))
            commits2m[(commit.authored_date, commit.author.name)]=(subject, commit.hexsha[:8])
            #print(commit.committed_date, commit.author.name, subject)

        # list and compare with commits of branch1
        # we need the commit and commit_next to be able to make a diff
        # start iterating before to enter into the loop
        commit_iterator=iter(repo.iter_commits(branch1))
        commit=next(commit_iterator) # we always have at least the common node
        for commit_next in commit_iterator:
            if commit.hexsha==common.hexsha:
                break # Stop when we have reached the common node

            # skip the commit if no files match the path filter
            if args.path:
                path_match=False
                for path in args.path:
                    diff=commit.diff(commit_next)
                    for entry in itertools.chain(diff.iter_change_type('M'), diff.iter_change_type('A'), diff.iter_change_type('D'), diff.iter_change_type('R')):
                        # print(entry.a_path, entry.b_path, entry.change_type, " - ", entry.diff)
                        p=entry.b_path if not entry.a_path else entry.a_path
                        if p.startswith(path):
                            path_match=True
                            break
                    if path_match:
                        break
                if not path_match:
                    commit=commit_next
                    continue

            subject=commit.message.split('\n', 1)[0]
            date=time.strftime("%Y-%m-%d %H:%M", time.gmtime(commit.authored_date))
            line='%s %s %s' % (date, commit.author.name, subject)
            alt_subject, alt_sha=None, None
            if args.sha:
                line='%s %s' % (commit.hexsha[:8], line)
            if (commit.authored_date, commit.author.name, subject) in commits2:
                prefix='='
            elif (commit.author.name, subject) in commits2b:
                prefix='~'
            elif (commit.authored_date, commit.author.name) in commits2m:
                prefix='§'
                alt_subject, alt_sha=commits2m[(commit.authored_date, commit.author.name)]
            else:
                prefix='+'
            print(prefix, line)
            if prefix=='§':
                line2='%s%s %s %s' % ((alt_sha+' ' if args.sha else ''), ' '*len(date), commit.author.name, alt_subject)
                print(prefix, line2)

            commit=commit_next

def print_prefix(prefix, content):
    for n, line in enumerate(content):
        print('%s:%04d %s' % (prefix, n, line))

def resolve_cherry_pick(args):
    repo=args.repo
    git_dir=repo.git_dir
    args.orig_head=os.path.join(git_dir, 'ORIG_HEAD')
    cherry_pick_head=open(args.cherry_pick_head).read().rstrip()
    orig_head=open(args.orig_head).read().rstrip()
    print('You are trying to apply commit (C) %s on top of commit (O) %s' % (cherry_pick_head, orig_head))
    index=repo.index
    print('The conflicting files are:')
    for i, blob in enumerate(index.unmerged_blobs()):
        print('%d: %s' % (i, blob, ))
    print('Display files content (M = conflicting commit, C = cherry-pick, O = original):')
    for i, blob in enumerate(index.unmerged_blobs()):
        # index.unmerged_blobs()[blob][0] is the cherry-pick version before the conflicting commit
        # index.unmerged_blobs()[blob][1] is the orig version version (before the conflicting commit)
        # index.unmerged_blobs()[blob][2] is the cherry-pick version after the commit
        fromlines=index.unmerged_blobs()[blob][0][1].data_stream.read().split('\n')
        tolines=index.unmerged_blobs()[blob][2][1].data_stream.read().split('\n')
        diff = difflib.unified_diff(fromlines, tolines, blob, blob, n=5)
        for line in diff:
            print('%d:M %s' % (i, line.rstrip()))
        print_prefix('%d:C' % (i,), index.unmerged_blobs()[blob][2][1].data_stream.read().split('\n'))
        print_prefix('%d:O' % (i,), index.unmerged_blobs()[blob][1][1].data_stream.read().split('\n'))
        #
        continue
        for j, item in enumerate(index.unmerged_blobs()[blob]):
            print(j, item[0], item[1].path)
            print(len(item[1].data_stream.read().split('\n')))
            open('/tmp/bgit.%d' % (item[0], ), 'w').write(item[1].data_stream.read())
#            for line in item[1].data_stream.read().split('\n'):
#                print(line)

def run_res_conflict(args):
    args.cherry_pick_head=os.path.join(args.repo.git_dir, 'CHERRY_PICK_HEAD')
    if os.path.isfile(args.cherry_pick_head):
        return resolve_cherry_pick(args)
    print('Not a cherry-pick issue! Sorry only cherry-pick is supported for now.', file=sys.stderr)

def run_version(args):
    print('GitPython:', git.__version__)
    print('git:', '.'.join(map(str, git.Git().version_info)))

mainparser=argparse.ArgumentParser(description='git utility for bacula')
subparsers=mainparser.add_subparsers(dest='command', metavar='', title='valid commands')

git_parser=argparse.ArgumentParser(add_help=False)
git_parser.add_argument('--git_dir', metavar='GIT-DIR', type=str, default='.', help='the directory with the .git sub dir')

cmp_branch_description="""Compare two branches given in the arguments.
Display all commits of the first branch starting from the node common to both
branches.
Commits that are in both branches with the same authored_date, author_name
and subject are prefixed with a '='.
Commits that are in both branches but with a different authored_date
are prefixed with a '~'.
Commits that have the same author_name and authored_date but a different subject
are prefixed with a '&' and both subject are shown.
Other commit are prefixed with a '+' that means that it was not found in
the second branch and could be added.
The first Branch can be a regex (prefixed with "re:") to compare a set of
branches to the second one"""

cmp_branch_legend="""= Commits that are in both branches with the same authored_date, author_name and  subject
~ Commits that are in both branches but with a different authored_date
& Commits that are in both branches with the same authored_date, author_name but  with a subject that is different
+ Commits are in %(branch1)s but not in %(branch2)s
"""

parser=subparsers.add_parser('cmp_branch', description=cmp_branch_description, parents=[git_parser, ],
    help='compare two branches, highligh commits missing in the second branch')

parser.add_argument('--switch', action='store_true', help='switch the two BRANCH parameters to ease use of xargs')
parser.add_argument('--sha', action='store_true', help='display the short sha1 of the commit')
parser.add_argument('--no-legend', action='store_true', help='don\'t display the legend')
parser.add_argument('--short-legend', action='store_true', help='don\'t display the legend')
parser.add_argument('--path', nargs=1, help='report only commits that modify this file or a file in this directory')
parser.add_argument('branch1', metavar='[re:]BRANCH-1', help='the first branch')
parser.add_argument('branch2', metavar='BRANCH-2', help='the second branch')
parser.set_defaults(func=run_cmp_branch)

res_conflict_help='help resolve conflic in the current branch'
parser=subparsers.add_parser('res_conflict', parents=[git_parser, ],
    description=res_conflict_help, help=res_conflict_help)
parser.set_defaults(func=run_res_conflict)

version_help="""shows version of git and GitPython"""
parser=subparsers.add_parser('version', description=version_help, help=version_help)
parser.set_defaults(func=run_version)


args=mainparser.parse_args()
args._parser=mainparser

if not args.command:
    mainparser.error('command argument required')

logging.getLogger().setLevel(logging.DEBUG)

add_console_logger()
#print(args.git_dir)
#print("logging into gitstat.log")
add_file_logger('gitstat.log')

# search the git repo
repo=None
if args.command in [ 'cmp_branch', 'res_conflict', ] and args.git_dir:
    if args.git_dir=='.':
        path=os.getcwd()
        while path and not os.path.isdir(os.path.join(path, '.git')):
            path=os.path.dirname(path)

        if path and os.path.isdir(os.path.join(path, '.git')):
            try:
                repo=git.Repo(path)
            except git.exc.InvalidGitRepositoryError:
                mainparser.error("git repository not found in %s" % (path,))
            else:
                args.git_dir=path
        else:
            mainparser.error("not .git directory found above %s" % (os.getcwd(),))

    else:
        try:
            repo=git.Repo(args.git_dir)
        except git.exc.InvalidGitRepositoryError:
            mainparser.error("git repository not found in %s" % (args.git_dir,))

args.repo=repo
args.func(args)

