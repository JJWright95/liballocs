import os, sys, re, subprocess, tempfile
from compilerwrapper import *

class AllocsCompilerWrapper(CompilerWrapper):

    def defaultL1AllocFns(self):
        return []

    def defaultFreeFns(self):
        return []

    def allL1OrWrapperAllocFns(self):
        if "LIBALLOCS_ALLOC_FNS" in os.environ:
            return self.defaultL1AllocFns() + [s for s in os.environ["LIBALLOCS_ALLOC_FNS"].split(' ') if s != '']
        else:
            return self.defaultL1AllocFns()

    def allSubAllocFns(self):
        if "LIBALLOCS_SUBALLOC_FNS" in os.environ:
            return [s for s in os.environ["LIBALLOCS_SUBALLOC_FNS"].split(' ') if s != '']
        else:
            return []

    def allAllocSzFns(self):
        if "LIBALLOCS_ALLOCSZ_FNS" in os.environ:
            return [s for s in os.environ["LIBALLOCS_ALLOCSZ_FNS"].split(' ') if s != '']
        else:
            return []

    def allAllocFns(self):
        return self.allL1OrWrapperAllocFns() + self.allSubAllocFns() + self.allAllocSzFns()

    def allL1FreeFns(self):
        if "LIBALLOCS_FREE_FNS" in os.environ:
            return self.defaultFreeFns() + [s for s in os.environ["LIBALLOCS_FREE_FNS"].split(' ') if s != '']
        else:
            return self.defaultFreeFns()

    def allSubFreeFns(self):
        if "LIBALLOCS_SUBFREE_FNS" in os.environ:
            return [s for s in os.environ["LIBALLOCS_SUBFREE_FNS"].split(' ') if s != '']
        else:
            return []

    def allFreeFns(self):
        return self.allL1FreeFns() + self.allSubFreeFns()

    def allWrappedSymNames(self):
        syms = []
        for allocFn in self.allAllocFns():
            if allocFn != '':
                m = re.match("(.*)\((.*)\)(.?)", allocFn)
                fnName = m.groups()[0]
                syms += [fnName]
        for freeFn in self.allSubFreeFns():
            if freeFn != '':
                m = re.match("(.*)\((.*)\)(->[a-zA-Z0-9_]+)", freeFn)
                fnName = m.groups()[0]
                syms += [fnName]
        for freeFn in self.allL1FreeFns():
            if freeFn != '':
                m = re.match("(.*)\((.*)\)", freeFn)
                fnName = m.groups()[0]
                syms += [fnName]
        return syms

    def findFirstUpperCase(self, s):
        allLower = s.lower()
        for i in range(0, len(s)):
            if s[i] != allLower[i]:
                return i
        return -1
        
    def getLibAllocsBaseDir(self):
        return os.path.dirname(__file__) + "/../"

    def getLibNameStem(self):
        return "allocs"
    
    def getDummyWeakObjectNameStem(self):
        return "dummyweaks"
    
    def getDummyWeakLinkArgs(self, outputIsDynamic, outputIsExecutable):
        if outputIsDynamic and outputIsExecutable:
            return [ "-Wl,--push-state", "-Wl,--no-as-needed", \
                    self.getLinkPath() + "/lib" + self.getLibNameStem() + "_" + self.getDummyWeakObjectNameStem() + ".so", \
                    "-Wl,--pop-state" ]
        elif outputIsDynamic and not outputIsExecutable:
            return [self.getLinkPath() + "/lib" + self.getLibNameStem() + "_" + self.getDummyWeakObjectNameStem() + ".o"]
        else:
            return []
    
    def getLdLibBase(self):
        return "-l" + self.getLibNameStem()
     
    def getLinkPath(self):
        return self.getLibAllocsBaseDir() + "/lib"
     
    def getRunPath(self):
        return self.getLinkPath()
    
    def getCustomCompileArgs(self, sourceInputFiles):
        gccOnlyOpts = ["-fvar-tracking-assignments"]
        commonOpts = ["-gdwarf-4", "-gstrict-dwarf", "-fno-omit-frame-pointer", "-ffunction-sections" ]
        if "CC_IS_CLANG" in os.environ:
            return commonOpts
        else:
            return commonOpts + gccOnlyOpts
    
    def fixupDotO(self, filename, errfile):
        self.debugMsg("Fixing up .o file: %s\n" % filename)
        if self.commandStopsBeforeObjectOutput():
            self.debugMsg("No .o file output.\n")
            return
        # do we need to unbind? 
        # MONSTER HACK: globalize a symbol if it's a named alloc fn. 
        # This is needed e.g. for SPEC benchmark bzip2
        with (self.makeErrFile(os.path.realpath(filename) + ".fixuplog", "w+") if not errfile else errfile) as errfile:

            # also link the file with the uniqtypes it references
            linkUsedTypesCmd = [self.getLibAllocsBaseDir() + "/tools/lang/c/bin/link-used-types", filename]
            self.debugMsg("Calling " + " ".join(linkUsedTypesCmd) + "\n")
            ret = subprocess.call(linkUsedTypesCmd, stderr=errfile)
            if ret != 0:
                self.print_errors(errfile)
                return ret  # give up now

            # Now deal with wrapped functions
            wrappedFns = self.allWrappedSymNames()
            self.debugMsg("Looking for wrapped functions that need unbinding\n")
            cmdstring = "nm -fbsd \"%s\" | grep -v '^[0-9a-f ]\+ U ' | egrep \"^[0-9a-f ]+ . (%s)$\" | sed 's/^[0-9a-f ]\+ . //'" \
                % (filename, "|".join(wrappedFns))
            self.debugMsg("cmdstring for objdump is " + cmdstring + "\n")
            grep_output = subprocess.Popen(["sh", "-c", cmdstring], stdout=subprocess.PIPE, stderr=errfile).communicate()[0]
            toUnbind = [l for l in grep_output.split("\n") if l != '']
            self.debugMsg("grep for defined alloc fns in %s returned %s\n" % (filename, str(toUnbind)))
            if grep_output != "":
                # we need to unbind. We unbind the allocsite syms
                # *and* --prefer-non-section-relocs. 
                # This will give us a file with __def_ and __ref_ symbols
                # for the allocation function. We then rename these to 
                # __real_ and __wrap_ respectively. 
                backup_filename = os.path.splitext(filename)[0] + ".backup.o"
                self.debugMsg("Found that we need to unbind symbols [%s]... making backup as %s\n" % \
                    (", ".join(toUnbind), backup_filename))
                cp_ret = subprocess.call(["cp", filename, backup_filename], stderr=errfile)
                if cp_ret != 0:
                    self.print_errors(errfile)
                    return cp_ret
                unbind_pairs = [["--unbind-sym", sym] for sym in toUnbind]
                unbind_cmd = ["objcopy", "--prefer-non-section-relocs"] \
                 + [opt for pair in unbind_pairs for opt in pair] \
                 + [filename]
                self.debugMsg("cmdstring for objcopy (unbind) is " + " ".join(unbind_cmd) + "\n")
                objcopy_ret = subprocess.call(unbind_cmd, stderr=errfile)
                if objcopy_ret != 0:
                    self.debugMsg("problem doing objcopy (unbind) (ret %d)\n" % objcopy_ret)
                    self.print_errors(errfile)
                    return objcopy_ret
                else:
                    # one more objcopy to rename the __def_ and __ref_ symbols
                    self.debugMsg("Renaming __def_ and __ref_ alloc symbols\n")
                    # instead of objcopying to replace __def_<sym> with <sym>,
                    # we use ld -r to define <sym> and __real_<sym> as *extra* symbols
                    ref_args = [["--redefine-sym", "__ref_" + sym + "=__wrap_" + sym] for sym in toUnbind]
                    objcopy_ret = subprocess.call(["objcopy", "--prefer-non-section-relocs"] \
                     + [opt for seq in ref_args for opt in seq] \
                     + [filename], stderr=errfile)
                    if objcopy_ret != 0:
                        self.print_errors(errfile)
                        return objcopy_ret
                    tmp_filename = os.path.splitext(filename)[0] + ".tmp.o"
                    cp_ret = subprocess.call(["cp", filename, tmp_filename], stderr=errfile)
                    if cp_ret != 0:
                        self.print_errors(errfile)
                        return cp_ret
                    def_args = [["--defsym", sym + "=__def_" + sym, \
                        "--defsym", "__real_" + sym + "=__def_" + sym, \
                        ] for sym in toUnbind]
                    ld_ret = subprocess.call(["ld", "-r"] \
                     + [opt for seq in def_args for opt in seq] \
                     + [tmp_filename, "-o", filename], stderr=errfile)
                    if ld_ret != 0:
                        self.debugMsg("problem doing ld -r (__real_ = __def_) (ret %d)\n" % ld_ret)
                        self.print_errors(errfile)
                        return ld_ret

            self.debugMsg("Looking for wrapped functions that need globalizing\n")
            # grep for local symbols -- a lower-case letter after the symname is the giveaway
            cmdstring = "nm -fposix --defined-only \"%s\" | egrep \"^(%s) [a-z] \"" \
                % (filename, "|".join(wrappedFns))
            self.debugMsg("cmdstring is %s\n" % cmdstring)
            grep_ret = subprocess.call(["sh", "-c", cmdstring], stderr=errfile)
            if grep_ret == 0:
                self.debugMsg("Found that we need to globalize\n")
                globalize_pairs = [["--globalize-symbol", sym] for sym in wrappedFns]
                objcopy_ret = subprocess.call(["objcopy"] \
                 + [opt for pair in globalize_pairs for opt in pair] \
                 + [filename])
                return objcopy_ret
            # no need to objcopy; all good
            self.debugMsg("No need to globalize\n")
            return 0

    def fixupLinkedObject(self, filename, is_exe, errfile):
        self.debugMsg("Fixing up linked output object: %s\n" % filename)
        wrappedFns = self.allWrappedSymNames()
        with (self.makeErrFile(os.path.realpath(filename) + ".fixuplog", "w+") if not errfile else errfile) as errfile:
            if is_exe:
                # for any allocator symbols that it defines, we must
                # 1. link in the callee-side stubs it needs
                #          (CAN'T do that now of course -- assume it's already been done)
                # 2. rename __wrap___real_<allocator> to <allocator> 
                #       and <allocator> to something arbitrary
                # In this way, local callers must already have had the extra 
                #   --wrap __real_<allocator> 
                # argument in order to get the callee instr. Internal calls are wired up properly.
                # Next, to allow lib-to-exe calls to hit the callee instr,
                # use objcopy to rename them
                # ARGH. This won't work because objcopy can't rename dynsys 
                pass
                # what allocator fns does it define globally?
                # grep for global symbols -- an upper-case letter after the symname is the giveaway
#                cmdstring = "nm -fposix --extern-only --defined-only \"%s\" | sed 's/ [A-Z] [0-9a-f ]\\+$//' | egrep \"^__wrap___real_(%s)$\""\
#                    % (filename, "|".join(wrappedFns))
#                self.debugMsg("cmdstring is %s\n" % cmdstring)
#                wrappedRealNames = subprocess.Popen(["sh", "-c", cmdstring], stderr=errfile, stdout=subprocess.PIPE).communicate()[0].split("\n")
#                self.debugMsg("output was %s\n" % wrappedRealNames)
#                if len(wrappedRealNames) > 0:
#                    # firstly do the bare (non-wrap-real-prefixed) ones
#                    bareNames = [sym[len("__wrap___real_"):] for sym in wrappedRealNames]
#                    self.debugMsg("Renaming __wrap___real_* alloc symbols: %s\n" % bareNames)
#                    redefine_args = [["--redefine-sym", sym + "=" + "__liballocs_bare_" + sym] \
#                        for sym in bareNames if sym != ""]
#                    objcopy_ret = subprocess.call(["objcopy"] \
#                     + [opt for seq in redefine_args for opt in seq] \
#                     + [filename], stderr=errfile)
#                    if objcopy_ret != 0:
#                        self.print_errors(errfile)
#                        return objcopy_ret
#                    redefine_args = [["--redefine-sym", "__wrap___real_" + sym + "=" + sym] \
#                       for sym in bareNames if sym != ""]
#                    objcopy_ret = subprocess.call(["objcopy"] \
#                     + [opt for seq in redefine_args for opt in seq] \
#                     + [filename], stderr=errfile)

    def getVerboseArgs(self):
        return []

    def getStubGenHeaderPath(self):
        return self.getLibAllocsBaseDir() + "/tools/stubgen.h"

    def getExtraLinkArgs(self, passedThroughArgs):
        return []
    
    def getStubGenCompileArgs(self):
        return []
        
    def doPostLinkMetadataBuild(self, outputFile):
        # We've just output an object, so invoke make to collect the allocsites, 
        # with our target name as the file we've just built, using ALLOCSITES_BASE 
        # to set the appropriate prefix
        if "ALLOCSITES_BASE" in os.environ:
            baseDir = os.environ["ALLOCSITES_BASE"]
        else:
            baseDir = "/usr/lib/allocsites"
        if os.path.exists(os.path.realpath(outputFile)):
            targetNames = [baseDir + os.path.realpath(outputFile) + ext \
                for ext in [".allocs", "-types.c", "-types.so", "-allocsites.c", "-allocsites.so"]]
            errfilename = baseDir + os.path.realpath(outputFile) + ".makelog"

            ret2 = 42
            with self.makeErrFile(errfilename, "w+") as errfile:
                cmd = ["make", "-C", self.getLibAllocsBaseDir() + "/tools", \
                    "-f", "Makefile.allocsites"] +  targetNames
                errfile.write("Running: " + " ".join(cmd) + "\n")
                ret2 = subprocess.call(cmd, stderr=errfile, stdout=errfile)
                errfile.write("Exit status was %d\n" % ret2)
                if (ret2 != 0 or "DEBUG_CC" in os.environ):
                    self.print_errors(errfile)
            return ret2
        else:
            return 1
    
    def main(self):
        # un-export CC from the env if it's set to allocscc, because 
        # we don't want to recursively crunchcc the -uniqtypes.c files
        # that this make invocation will be compiling for us.
        # NOTE that we really do mean CC and not CXX or FC here, because
        # all the stuff we build ourselves is built from C.
        #if "CC" in os.environ and os.environ["CC"].endswith(os.path.basename(sys.argv[0])):
        if "CC" in os.environ:# and os.environ["CC"].endswith(os.path.basename(sys.argv[0])):
           del os.environ["CC"]
        self.debugMsg(sys.argv[0] + " called with args  " + " ".join(sys.argv) + "\n")

        (sourceInputFiles, objectInputFiles, outputFile, isLinkCommand) = self.parseInputAndOutputFiles(sys.argv)
        if isLinkCommand:
            self.debugMsg("We are a link command\n")
        else:
            self.debugMsg("We are not a link command\n")

        # If we're a linker command, then we have to handle allocation functions
        # specially.
        # Each allocation function, e.g. xmalloc, is linked with --wrap.
        # If we're outputting a shared library, we leave it like this,
        # with dangling references to __wrap_xmalloc,
        # and an unused implementation of __real_xmalloc.
        # If we're outputting an executable, 
        # then we link a thread-local variable "__liballocs_current_allocsite"
        # into the executable,
        # and for each allocation function, we link a generated stub.

        allocsccCustomArgs = self.getCustomCompileArgs(sourceInputFiles)
        
        # we add -ffunction-sections to ensure that references to malloc functions 
        # generate a relocation record -- since a *static, address-taken* malloc function
        # might otherwise have its address taken without a relocation record. 
        # Moreover, we want the relocation record to refer to the function symbol, not
        # the section symbol. We handle this by using my hacked-in --prefer-non-section-relocs
        # objcopy option *if* we do symbol unbinding.

        mallocWrapArgs = []
        for sym in self.allWrappedSymNames():
            mallocWrapArgs += ["-Wl,--wrap," + sym]

        linkArgs = []
        if isLinkCommand:
            # we need to build the .o files first, 
            # then link in the uniqtypes they reference, 
            # then resume linking these .o files
            if len(sourceInputFiles) > 0:
                self.debugMsg("Making .o files first from " + " ".join(sourceInputFiles) + "\n")
                passedThroughArgs = self.makeDotOAndPassThrough(sys.argv, allocsccCustomArgs, \
                    sourceInputFiles)
            else:
                passedThroughArgs = sys.argv[1:]
            
            buildingSharedObject = ("-shared" in passedThroughArgs) or ("-G" in passedThroughArgs)

            # we pass through the input .o and .a files
            self.debugMsg("passedThroughArgs is %s\n" % " ".join(passedThroughArgs))
            #passedThroughArgs += objectInputFiles

            # we need to wrap each allocation function
            self.debugMsg("allocscc doing linking\n")
            passedThroughArgs += mallocWrapArgs
            # we need to export-dynamic, s.t. __is_a is linked from liballocs
            linkArgs += ["-Wl,--export-dynamic"]
            # if we're building an executable, append the magic objects
            # -- and link with the noop *shared* library, to be interposable.
            # Recall that if we're building a shared object, we don't need to
            # link in the alloc stubs, because we will use symbol interposition
            # to get control into our stubs. OH, but wait. We still have custom
            # allocation functions, and we want them to set the alloc site. 
            # So we do want to link in the wrappers. Do we need to rewrite
            # references to __real_X after this?
            if not "-r" in passedThroughArgs \
                and not "-Wl,-r" in passedThroughArgs:
                
                # \
                # and not "-shared" in passedThroughArgs \
                # and not "-G" in passedThroughArgs:

                # make a temporary file for the stubs
                # -- we derive the name from the output binary, and bail if it's taken?
                # NO, because we want repeat builds to succeed
                stubsfile_name = outputFile + ".allocstubs.c"
                with open(stubsfile_name, "w") as stubsfile:
                    self.debugMsg("stubsfile is %s\n" % stubsfile.name)
                    stubsfile.write("#include \"" + self.getStubGenHeaderPath() + "\"\n")

                    def writeArgList(fnName, fnSig):
                        stubsfile.write("#define arglist_%s(make_arg) " % fnName)
                        ndx = 0
                        for c in fnSig: 
                            if ndx != 0:
                                stubsfile.write(", ")
                            stubsfile.write("make_arg(%d, %c)" % (ndx, c))
                            ndx += 1
                        stubsfile.write("\n")
                        stubsfile.write("#define arglist_nocomma_%s(make_arg) " % fnName)
                        ndx = 0
                        for c in fnSig: 
                            stubsfile.write("make_arg(%d, %c)" % (ndx, c))
                            ndx += 1
                        stubsfile.write("\n")

                    for allocFn in self.allAllocFns():
                        m = re.match("(.*)\((.*)\)(.?)", allocFn)
                        fnName = m.groups()[0]
                        fnSig = m.groups()[1]
                        retSig = m.groups()[2]
                        writeArgList(fnName, fnSig)
                        sizendx = self.findFirstUpperCase(fnSig)
                        if sizendx != -1:
                            # it's a size char, so flag that up
                            stubsfile.write("#define size_arg_%s make_argname(%d, %c)\n" % (fnName, sizendx, fnSig[sizendx]))
                        else:
                            # If there's no size arg, it's a typed allocation primitive, and 
                            # the size is the size of the thing it returns. How can we get
                            # at that? Have we built the typeobj already? No, because we haven't
                            # even built the binary. But we have built the .o, including the
                            # one containing the "real" allocator function. Call a helper
                            # to do this.
                            size_find_command = [self.getLibAllocsBaseDir() \
                                + "/tools/find-allocated-type-size", fnName] + [ \
                                objfile for objfile in passedThroughArgs if objfile.endswith(".o")]
                            self.debugMsg("Calling " + " ".join(size_find_command) + "\n")
                            outp = subprocess.Popen(size_find_command, stdout=subprocess.PIPE).communicate()[0]
                            self.debugMsg("Got output: " + outp + "\n")
                            # we get lines of the form <number> \t <explanation>
                            # so just chomp the first number
                            outp_lines = outp.split("\n")
                            if (len(outp_lines) < 1 or outp_lines[0] == ""):
                                self.debugMsg("No output from %s" % " ".join(size_find_command))
                                return 1  # give up now
                            sz = int(outp_lines[0].split("\t")[0])
                            stubsfile.write("#define size_arg_%s %d\n" % (fnName, sz))
                        if allocFn in self.allL1OrWrapperAllocFns():
                            stubsfile.write("make_wrapper(%s, %s)\n" % (fnName, retSig))
                        elif allocFn in self.allAllocSzFns():
                            stubsfile.write("make_size_wrapper(%s, %s)\n" % (fnName, retSig))
                        else:
                            stubsfile.write("make_suballocator_alloc_wrapper(%s, %s)\n" % (fnName, retSig))
                        stubsfile.flush()
                    # also do subfree wrappers
                    for freeFn in self.allSubFreeFns():
                        m = re.match("(.*)\((.*)\)(->([a-zA-Z0-9_]+))", freeFn)
                        fnName = m.groups()[0]
                        fnSig = m.groups()[1]
                        allocFnName = m.groups()[3]
                        ptrndx = fnSig.find('P')
                        if ptrndx != -1:
                            # it's a ptr, so flag that up
                            stubsfile.write("#define ptr_arg_%s make_argname(%d, %c)\n" % (fnName, ptrndx, fnSig[ptrndx]))
                        writeArgList(fnName, fnSig)
                        stubsfile.write("make_suballocator_free_wrapper(%s, %s)\n" % (fnName, allocFnName))
                        stubsfile.flush()
                    # also do free (non-sub) -wrappers
                    for freeFn in self.allL1FreeFns():
                        m = re.match("(.*)\((.*)\)", freeFn)
                        fnName = m.groups()[0]
                        fnSig = m.groups()[1]
                        ptrndx = fnSig.find('P')
                        if ptrndx != -1:
                            # it's a ptr, so flag that up
                            stubsfile.write("#define ptr_arg_%s make_argname(%d, %c)\n" % (fnName, ptrndx, fnSig[ptrndx]))
                        writeArgList(fnName, fnSig)
                        stubsfile.write("make_free_wrapper(%s)\n" % fnName)
                        stubsfile.flush()
                    # now we compile the C file ourselves, rather than cilly doing it, 
                    # because it's a special magic stub
                    stubs_pp = os.path.splitext(stubsfile.name)[0] + ".i"
                    stubs_bin = os.path.splitext(stubsfile.name)[0] + ".o"
                    # We *should* pass through some options here, like -DNO_TLS. 
                    # To do "mostly the right thing", we preprocess with 
                    # most of the user's options, 
                    # then compile with a more tightly controlled set
                    extraFlags = self.getStubGenCompileArgs()
                    if buildingSharedObject:
                        extraFlags += ["-fPIC"]
                    else:
                        pass
                    stubs_pp_cmd = ["cc", "-std=c11", "-E", "-Wp,-P"] + extraFlags + ["-o", stubs_pp, \
                        "-I" + self.getLibAllocsBaseDir() + "/tools"] \
                        + [arg for arg in passedThroughArgs if arg.startswith("-D")] \
                        + [stubsfile.name]
                    self.debugMsg("Preprocessing stubs file %s to %s with command %s\n" \
                        % (stubsfile.name, stubs_pp, " ".join(stubs_pp_cmd)))
                    ret_stubs_pp = subprocess.call(stubs_pp_cmd)
                    if ret_stubs_pp != 0:
                        self.debugMsg("Could not preproces stubs file %s: compiler returned %d\n" \
                            % (stubsfile.name, ret_stubs_pp))
                        exit(1)
                    # now erase the '# ... file ' lines that refer to our stubs file,
                    # and add some line breaks
                    # -- HMM, why not just erase all #line directives? i.e. preprocess with -P?
                    # We already do this.
                    # NOTE: the "right" thing to do is keep the line directives
                    # and replace the ones pointing to stubgen.h
                    # with ones pointing at the .i file itself, at the appropriate line numbers.
                    # This is tricky because our insertion of newlines will mess with the
                    # line numbers.
                    # Though, actually, we should only need a single #line directive.
                    # Of course this is only useful if the .i file is kept around.
                    stubs_sed_cmd = ["sed", "-r", "-i", "s^#.*allocs.*/stubgen\\.h\" *[0-9]* *$^^\n " \
                    + "/__real_|__wrap_|__current_/ s^[;\\{\\}]^&\\n^g", stubs_pp]
                    ret_stubs_sed = subprocess.call(stubs_sed_cmd)
                    if ret_stubs_sed != 0:
                        self.debugMsg("Could not sed stubs file %s: sed returned %d\n" \
                            % (stubs_pp, ret_stubs_sed))
                        exit(1)
                    stubs_cc_cmd = ["cc", "-std=c11", "-g"] + extraFlags + ["-c", "-o", stubs_bin, \
                        "-I" + self.getLibAllocsBaseDir() + "/tools", \
                        stubs_pp]
                    self.debugMsg("Compiling stubs file %s to %s with command %s\n" \
                        % (stubs_pp, stubs_bin, " ".join(stubs_cc_cmd)))
                    stubs_output = None
                    try:
                        stubs_output = subprocess.check_output(stubs_cc_cmd, stderr=subprocess.STDOUT)
                    except subprocess.CalledProcessError, e:
                        self.debugMsg("Could not compile stubs file %s: compiler returned %d\n" \
                            % (stubs_pp, e.returncode))
                        if stubs_output:
                            self.debugMsg("Compiler said: %s\n" % stubs_output)
                        exit(1)
                    if stubs_output != "":
                        self.debugMsg("Compiling stubs file %s: compiler said \"%s\"\n" \
                            % (stubs_pp, stubs_output))

                    # CARE: we must insert the wrapper object on the cmdline *before* any 
                    # archive that is providing the wrapped functions. The easiest way
                    # is to insert it first, it appears.
                    linkArgs = [stubs_bin] + linkArgs + self.getExtraLinkArgs(passedThroughArgs)
                    linkArgs += ["-L" + self.getLinkPath()]
                    if not "-static" in passedThroughArgs and not "-Bstatic" in passedThroughArgs \
                        and not "-r" in passedThroughArgs and not "-Wl,-r" in passedThroughArgs:
                        # we're building a dynamically linked executable
                        linkArgs += ["-Wl,-rpath," + self.getRunPath()]
                        if "LIBALLOCS_USE_PRELOAD" in os.environ and os.environ["LIBALLOCS_USE_PRELOAD"] == "no":
                            linkArgs += [self.getLdLibBase()]
                        else: # FIXME: weak linkage one day; FIXME: don't clobber as-neededness
                            # HACK: why do we need --as-needed? try without.
                            # NO NO NO! linker chooses the path of weakness, i.e. instead of 
                            # using symbols from _noop.so, uses 0 and doesn't depend on noop.
                            # AHA: the GNU linker has this handy --push-state thing...
                            linkArgs += self.getDummyWeakLinkArgs(True, True)
                    else:
                        # we're building a statically linked executable
                        if "LIBALLOCS_USE_PRELOAD" in os.environ and os.environ["LIBALLOCS_USE_PRELOAD"] == "no":
                            linkArgs += [self.getLdLibBase()]
                        else:
                            # no load-time overriding; do link-time overriding 
                            # by using the full liballocs library in archive form
                            linkArgs += [self.getLinkPath() + "/lib" + self.getLibNameStem() + ".a"]
            else:
                if not "-r" in passedThroughArgs and not "-Wl,-r" in passedThroughArgs:
                    # We're building a shared library, so simply add liballocs_noop.o; 
                    # only link directly if we're disabling the preload approach
                    if "LIBALLOCS_USE_PRELOAD" in os.environ and os.environ["LIBALLOCS_USE_PRELOAD"] == "no":
                        linkArgs += ["-L" + self.getLinkPath()]
                        linkArgs += ["-Wl,-rpath," + self.getRunPath()]
                        linkArgs += [getLdLibBase()]
                    else: # FIXME: weak linkage one day....
                        linkArgs += self.getDummyWeakLinkArgs(True, False)
                    # note: we leave the shared library with 
                    # dangling dependencies on __wrap_
                    # and unused __real_
                else:
                    # we're building a relocatable object. Don't add anything, because
                    # we'll get multiple definition errors. Instead assume that allocscc
                    # will be used to link the eventual output, so that is the right time
                    # to fill in the extra undefineds that we're inserting.
                    pass
            
            linkArgs += ["-ldl"]

        else:
            passedThroughArgs = sys.argv[1:]

        verboseArgs = self.getVerboseArgs()
        
        argsToExec = verboseArgs + allocsccCustomArgs \
        + passedThroughArgs \
        + linkArgs
        self.debugMsg("about to run cilly with args: " + " ".join(argsToExec) + "\n")
        self.debugMsg("passedThroughArgs is: " + " ".join(passedThroughArgs) + "\n")
        self.debugMsg("allocsccCustomArgs is: " + " ".join(allocsccCustomArgs) + "\n")
        self.debugMsg("linkArgs is: " + " ".join(linkArgs) + "\n")

        ret1 = self.runUnderlyingCompiler(sourceInputFiles, argsToExec)

        if ret1 != 0:
            # we didn't succeed, so quit now
            return ret1

        # We did succeed, so we need to fix up the output binary's 
        # __uniqtype references to the actual binary-compatible type
        # definitions which the compiler generated.

        if not isLinkCommand:
            if not self.commandStopsBeforeObjectOutput():
                if outputFile:
                    # we have a single named output file
                    ret2 = self.fixupDotO(outputFile, None)
                    return ret2
                else:
                    # no explicit output file; the compiler output >=1 .o files, one for each input
                    for outputFilename in [nameStem + ".o" \
                        for (nameStem, nameExtension) in map(os.path.splitext, sourceInputFiles)]:
                        self.fixupDotO(outputFilename, None)

        else: # isLinkCommand
            if not "-r" in passedThroughArgs and not "-Wl,-r" in passedThroughArgs:
                self.fixupLinkedObject(outputFile, not buildingSharedObject, None)
                return self.doPostLinkMetadataBuild(outputFile)
            else:
                return 0

    # expose base class methods to derived classes
    def parseInputAndOutputFiles(self, args):
        return CompilerWrapper.parseInputAndOutputFiles(self, args)

    def makeDotOAndPassThrough(self, argv, customArgs, inputFiles):
        return CompilerWrapper.makeDotOAndPassThrough(self, argv, customArgs, inputFiles)
