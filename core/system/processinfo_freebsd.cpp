#include <fcntl.h>
#include <kvm.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <paths.h>

#include "agosystem.h"

static int fscale = -1;

static bool init() {
    // get kern.fscale (from ps(1) nlist.c)
    size_t size = sizeof(fscale);
    if (sysctlbyname("kern.fscale", &fscale, &size, NULL, 0) == -1 ||
            size != sizeof(fscale)) {
        AGO_ERROR() << "failed to get kern.fscale";
        return false;
    }

    return true;
}

/**
 * From ps(1):
 * https://github.com/freebsd/freebsd/blob/master/bin/ps/print.c
 */
double
getpcpu(const struct kinfo_proc *ki_p)
{
#define fxtofl(fixpt)   ((double)(fixpt) / fscale)

    /* XXX - I don't like this */
    if (ki_p->ki_swtime == 0 || (ki_p->ki_flag & P_INMEM) == 0)
        return (0.0);

    return (100.0 * fxtofl(ki_p->ki_pctcpu));
}


void AgoSystem::getProcessInfo()
{
    if(fscale < 0) {
        if(!init())
            return;
    }

    kvm_t *kd;
    char errbuf[_POSIX2_LINE_MAX];
    kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errbuf);
    if(kd == 0) {
        AGO_ERROR() << "Failed to open KVM: " << errbuf;
        return;
    }

    struct kinfo_proc *kp;
    int nentries = -1;
    kp = kvm_getprocs(kd, KERN_PROC_PROC, 0, &nentries);
    if ((kp == NULL && nentries > 0) || (kp != NULL && nentries < 0)){
        AGO_ERROR() << "Failed to get process list" << kvm_geterr(kd);
    }

    if(nentries > 0) {
        for (int i = nentries; --i >= 0; ++kp) {
            std::string procName(kp->ki_comm);
            if( processes.find(procName) == processes.end() )
                continue;

            AGO_DEBUG() << "Found process " << kp->ki_pid << ": " << procName;
            qpid::types::Variant::Map stats = processes[procName].asMap();
            qpid::types::Variant::Map cs = stats["currentStats"].asMap();
            qpid::types::Variant::Map ls = stats["lastStats"].asMap();

            // update current stats
            // In FreeBSD we can get CPU as percentage directly. The linux version
            // calculates this manually from scheduled times, which gives user+system CPU.
            // However, we only sum of those anyway, so just use system provided CPU value.
            cs["ucpu"] = getpcpu(kp);
            cs["scpu"] = 0;

            cs["vsize"] = (uint64_t)kp->ki_size;
            cs["rss"] = (uint64_t)kp->ki_rssize * getpagesize();

            AGO_DEBUG() << cs;

            //update last stats
            stats["lastStats"] = cs;

            //finalize
            stats["currentStats"] = cs;
            stats["running"] = true;
            processes[procName] = stats;
        }
    }

    kvm_close(kd);
}
