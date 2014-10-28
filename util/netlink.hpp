#ifndef __NETLINK_H__
#define __NETLINK_H__

#include <string>
#include <functional>
#include <memory>

#include "error.hpp"

struct nl_sock;
struct rtnl_link;
struct nl_cache;

enum class ETclassStat {
    Packets,
    Bytes,
    Drops,
    Overlimits
};

uint32_t TcHandle(uint16_t maj, uint16_t min);
uint32_t TcRootHandle();
uint16_t TcMajor(uint32_t handle);

class TNl {
    NO_COPY_CONSTRUCT(TNl);

    struct nl_sock *Sock = nullptr;
    struct nl_cache *LinkCache = nullptr;

public:

    TNl() {}
    ~TNl() { Disconnect(); }

    TError Connect();
    void Disconnect();
    std::vector<std::string> FindLink(int flags);

    static void EnableDebug(bool enable);

    struct nl_sock *GetSock() { return Sock; }
    struct nl_cache *GetCache() { return LinkCache; }

    TError GetDefaultLink(std::string &link);
    static TError FindDefaultLink(std::string &link);
};

class TNlLink {
    NO_COPY_CONSTRUCT(TNlLink);

    std::shared_ptr<TNl> Nl;
    std::string Name;

public:
    struct rtnl_link *Link = nullptr;

    TNlLink(std::shared_ptr<TNl> nl, const std::string &name) : Nl(nl), Name(name) {}
    ~TNlLink();
    TError Load();

    TError Remove();
    TError Up();
    TError ChangeNs(const std::string &newName, int pid);
    bool Valid();
    int FindIndex(const std::string &device);
    TError AddMacVlan(const std::string &master,
                      const std::string &type, const std::string &hw);
    TError AddMacVlan(const std::string &master,
                      const std::string &type, const std::string &hw,
                      int nsPid);
    const std::string &GetName() { return Name; }

    static bool ValidMacVlanType(const std::string &type);
    static bool ValidMacAddr(const std::string &hw);

    int GetIndex();
    struct rtnl_link *GetLink() { return Link; }
    struct nl_sock *GetSock() { return Nl->GetSock(); }

    void LogObj(const std::string &prefix, void *obj);
    void LogCache(struct nl_cache *cache);

    static TError Exec(std::string name, std::function<TError(std::shared_ptr<TNlLink> Link)> f);
};

class TNlClass {
    NO_COPY_CONSTRUCT(TNlClass);

    std::shared_ptr<TNlLink> Link;
    const uint32_t Parent, Handle;

public:
    TNlClass(std::shared_ptr<TNlLink> link, uint32_t parent, uint32_t handle) : Link(link), Parent(parent), Handle(handle) {}

    TError Create(uint32_t prio, uint32_t rate, uint32_t ceil);
    TError Remove();
    TError GetStat(ETclassStat stat, uint64_t &val);
    TError GetProperties(uint32_t &prio, uint32_t &rate, uint32_t &ceil);
    bool Exists();
};

class TNlHtb {
    NO_COPY_CONSTRUCT(TNlHtb);

    std::shared_ptr<TNlLink> Link;
    const uint32_t Parent, Handle;

public:
    TNlHtb(std::shared_ptr<TNlLink> link, uint32_t parent, uint32_t handle) : Link(link), Parent(parent), Handle(handle) {}
    TError Create(uint32_t defaultClass);
    TError Remove();
    bool Exists();
};

class TNlCgFilter {
    NO_COPY_CONSTRUCT(TNlCgFilter);

    const int FilterPrio = 10;
    const char *FilterType = "cgroup";

    std::shared_ptr<TNlLink> Link;
    const uint32_t Parent, Handle;

public:
    TNlCgFilter(std::shared_ptr<TNlLink> link, uint32_t parent, uint32_t handle) : Link(link), Parent(parent), Handle(handle) {}
    TError Create();
    bool Exists();
    TError Remove();
};

static inline bool ValidLink(const std::string &name) {
    return TNlLink::Exec(name,
        [&](std::shared_ptr<TNlLink> link) {
            if (link->Valid())
                return TError::Success();
            else
                return TError(EError::Unknown, "");
        }) == TError::Success();
}

#endif
