#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <vector>
#include <set>
#include <execution>
#include <algorithm>
#include <thread>
#include <memory>

#include <openssl/sha.h>
#include <tbb/iterators.h>

#include "hoytech/hex.h"
#include "hoytech/error.h"



static int nextFileId = 1;

template< typename T >
struct FVector {
    FVector() {}

    FVector(std::string_view dir, std::string_view name) {
        path = std::string(dir);
        path += "/";
        path += name;
        path += ".";
        path += std::to_string(nextFileId++);
        path += ".dat";
    }

    ~FVector() {
        if (ptr) {
            munmap(ptr, sizeof(T) * _capacity);
            unlink(path.c_str());
            ptr = nullptr;
            _capacity = _size = 0;
        }
    }

    FVector(const FVector &) = delete;
    FVector& operator= (const FVector &) = delete;

    T *begin() { return ptr; }
    T *end() { return ptr + _size; }
    size_t size() { return _size; }
    size_t capacity() { return _capacity; }
    T operator [](size_t i) const { return ptr[i]; }
    T &operator [](size_t i) { return ptr[i]; }

    void reserve(size_t newCapacity) {
        if (ptr) {
            if (_capacity >= newCapacity) return;

            if (munmap(ptr, sizeof(T) * _capacity) == -1) throw hoytech::error("munmap failed: ", strerror(errno));
            ptr = nullptr;
            _capacity = 0;
        }

        if (!ptr) {
            if (!path.size()) throw hoytech::error("path not configured");

            if (newCapacity == 0) return;

            _capacity = newCapacity;

            int fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);
            if (fd == -1) throw hoytech::error("couldn't open file: ", path);

            size_t bytes = sizeof(T) * _capacity;

            if (ftruncate(fd, bytes) == -1) {
                close(fd);
                throw hoytech::error("couldn't truncate file: ", strerror(errno));
            }

            char *p = (char*)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (p == MAP_FAILED) {
                close(fd);
                throw hoytech::error("couldn't mmap file: ", strerror(errno));
            }

            ptr = reinterpret_cast<T*>(p);

            close(fd);
        }
    }

    void resize(size_t newSize) {
        reserve(newSize);
        _size = newSize;
    }

    void clear() {
        _size = 0;
    }

    void push_back(const T &value) {
        if (_size == _capacity) reserve(_size == 0 ? 1048576 : _size * 2);
        ptr[_size] = value;
        _size++;
    }

    T &back() {
        if (_size == 0) throw hoytech::error("FVector empty");
        return ptr[_size - 1];
    }

  private:
    size_t _size = 0;
    size_t _capacity = 0;
    T *ptr = nullptr;
    std::string path;
};




struct Elem {
    uint8_t buf[32];
    uint64_t id;

    void add(uint8_t *otherBuf, uint64_t v = 0) {
        for (size_t i = 0; i < 32; i++) {
            v = buf[i] + otherBuf[i] + v;
            buf[i] = v & 0xff;
            v >>= 8;
        }
    }

    void negateAndAdd(uint8_t *target) {
        for (size_t i = 0; i < 32; i++) {
            buf[i] = ~buf[i];
        }

        add(target, 1);
    }

    void setToZero() {
        memset(buf, '\0', sizeof(buf));
        id = 0;
    }

    std::string_view sv() const {
        return std::string_view(reinterpret_cast<const char*>(buf), 32);
    }

    std::string hex() const {
        std::string o = std::string(sv());
        std::reverse(o.begin(), o.end());
        return hoytech::to_hex(o);
    }
} __attribute__((packed));


struct FoundElem {
    uint64_t localId;
    uint64_t parentId1;
    uint64_t parentId2;
} __attribute__((packed));




struct Stage {
    std::string memDir;
    uint8_t *target;
    uint64_t stageNum;
    size_t stopMerging;

    bool isFinalStage = false;
    uint64_t currId = 1;
    std::string nameBase;

    std::unique_ptr<FVector<Elem>> inbox;
    std::unique_ptr<FVector<Elem>> big;
    std::unique_ptr<FVector<FoundElem>> found;

    Stage(const std::string &memDir, uint8_t *target, uint64_t stageNum, size_t stopMerging) : memDir(memDir), target(target), stageNum(stageNum), stopMerging(stopMerging) {
        nameBase += "stage-";
        nameBase += std::to_string(stageNum);
        nameBase += "-";

        inbox = std::make_unique<FVector<Elem>>(memDir, nameBase + "inbox");
        big = std::make_unique<FVector<Elem>>(memDir, nameBase + "big");
        found = std::make_unique<FVector<FoundElem>>(memDir, nameBase + "found");
    }

    std::set<uint64_t> expand(FVector<Elem> &outbox) {
        size_t origFound = found->size();
        bool doMerge = big->size() < stopMerging;

        {
            std::string o = "Status: inbox = ";
            o += std::to_string(inbox->size());
            o += " big = ";
            o += std::to_string(big->size());
            o += " found = ";
            o += std::to_string(found->size());
            logMsg(o);
        }

        auto cmp = [](const auto &a, const auto &b){ return memcmp(a.buf, b.buf, 32) < 0; };

        logMsg("Building negation");

        auto inboxNegs = std::make_unique<FVector<Elem>>(memDir, nameBase + "inboxNegs");
        inboxNegs->resize(inbox->size());

        std::for_each(std::execution::par, tbb::counting_iterator<size_t>(0), tbb::counting_iterator<size_t>(inbox->size()), [&](const size_t i) {
            (*inboxNegs)[i] = (*inbox)[i];
            (*inboxNegs)[i].negateAndAdd(target);
        });

        if (doMerge) {
            logMsg("Sort inbox");
            std::sort(std::execution::par_unseq, inbox->begin(), inbox->end(), cmp);
        }

        logMsg("Sort inboxNegs");
        std::sort(std::execution::par_unseq, inboxNegs->begin(), inboxNegs->end(), cmp);

        // Merge into big

        if (doMerge) {
            if (big->size()) {
                logMsg("Merging into big");

                auto newBig = std::make_unique<FVector<Elem>>(memDir, nameBase + "big");
                newBig->resize(big->size() + inbox->size());

                std::merge(big->begin(), big->end(), inbox->begin(), inbox->end(), newBig->begin(), cmp);
                std::swap(big, newBig);
            } else {
                logMsg("Moving inbox to big");

                std::swap(big, inbox);
            }
        }

        // Find matches

        logMsg("Finding matches");

        {
            auto currBig = big->begin();
            auto currInboxNeg = inboxNegs->begin();

            while (currBig != big->end() && currInboxNeg != inboxNegs->end()) {
                if (currBig->sv().starts_with(currInboxNeg->sv().substr(0, (stageNum + 1) * 4))) {
                    if (isFinalStage) {
                        return { currBig->id, currInboxNeg->id };
                    }

                    found->push_back(FoundElem(currId, currBig->id, currInboxNeg->id));

                    auto foundItem = *currInboxNeg;
                    foundItem.negateAndAdd(target);
                    foundItem.add(currBig->buf);
                    foundItem.id = currId;
                    outbox.push_back(foundItem);

                    //std::cout << "FOUND: " << currBig->id << " + " << currInboxNeg->id << " -> " << currId << " / " << outbox.back().hex() << std::endl;
                    currId++;
                }

                if (cmp(*currInboxNeg, *currBig)) ++currInboxNeg;
                else ++currBig;
            }
        }

        {
            auto newInbox = std::make_unique<FVector<Elem>>(memDir, nameBase + "inbox");
            std::swap(inbox, newInbox);
        }

        logMsg(std::string("newly found: ") + std::to_string(found->size() - origFound));

        return {};
    }

    void logMsg(const std::string &msg) {
        std::cout << std::string(2 * (stageNum + 1), ' ');
        std::cout << "[" << stageNum << "] ";
        std::cout << msg << std::endl;
    }
};


struct Generator {
    std::string memDir;
    size_t batchSize;
    size_t mergeLimit;

    // Internal
    uint64_t currSeed = 1;
    std::vector<Stage> stages;

    Generator(const std::string &memDir, size_t batchSize, size_t mergeLimit) : memDir(memDir), batchSize(batchSize), mergeLimit(mergeLimit) {}

    std::set<uint64_t> run(size_t numStages, uint8_t *target) {
        if (stages.size()) std::cout << "Resuming at stage " << stages.size() << std::endl;

        for (uint64_t i = stages.size(); i < numStages; i++) {
            size_t stop = mergeLimit;
            stages.emplace_back(memDir, target, i, stop);
        }
        stages[numStages - 1].isFinalStage = true;
        stages.emplace_back(memDir, target, 0, 0); // dummy stage so final stage has somewhere to connect its outbox

        std::cout << "merge limits:  ";
        for (uint64_t i = 0; i < numStages; i++) std::cout << " " << i << ":" << stages[i].stopMerging;
        std::cout << std::endl;

        while (1) {
            std::cout << "Sizes:";
            for (size_t i = 0; i < numStages; i++) {
                if (stages[i].big->size() == 0) break;
                std::cout << " " << i << ":" << stages[i].big->size();
                std::cout << (stages[i].big->size() >= stages[i].stopMerging ? "(done)" : "");
            }
            std::cout << std::endl;

            for (ssize_t i = numStages - 1; i >= 0; i--) {
                if (i == 0 && stages[0].inbox->size() == 0) {
                    size_t numRecs = batchSize;
                    stages[0].inbox->resize(numRecs);

                    std::cout << "Generating " << numRecs << " fresh hashes" << std::endl;
                    populateHashes(stages[0].inbox->begin(), numRecs);
                }

                if (i == 0 || stages[i].inbox->size() > 0) {
                    auto ids = stages[i].expand(*stages[i + 1].inbox);
                    if (ids.size()) return recoverSeeds(ids, i - 1);
                    break;
                }
            }

            std::cout << "------------------" << std::endl;
        }
    }

    void populateHashes(Elem *elemsPtr, size_t numRecs) {
        std::for_each(std::execution::par, tbb::counting_iterator<size_t>(0), tbb::counting_iterator<size_t>(numRecs), [&](const size_t i) {
            Elem &e = *(elemsPtr + i);

            uint64_t seed = currSeed + i;
            std::string buf = std::to_string(seed);
            SHA256(reinterpret_cast<unsigned char*>(buf.data()), buf.size(), reinterpret_cast<unsigned char*>(e.buf));
            std::reverse(e.buf, e.buf + 32);
            e.id = seed;
        });

        currSeed += numRecs;
    }

    std::set<uint64_t> recoverSeeds(std::set<uint64_t> &ids, ssize_t i) {
        for (; i >= 0; i--) {
            std::set<uint64_t> newIds;

            for (auto id : ids) {
                auto lower = std::lower_bound(stages[i].found->begin(), stages[i].found->end(), id,
                                [](auto &a, uint64_t v){ return a.localId < v; });
                if (lower == stages[i].found->end() || lower->localId != id) throw hoytech::error("Unable to find id ", id, " in stage ", i);
                newIds.insert(lower->parentId1);
                newIds.insert(lower->parentId2);
            }

            std::swap(ids, newIds);
        }

        return ids;
    }
};


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: bday <path-to-memdir> [target]" << std::endl;
        return 1;
    }


    size_t numStages = 8;
    size_t batchSize = 500'000'000;
    size_t mergeLimit = 4'000'000'000UL;


    std::string target(numStages * 4, '\0');

    if (argc >= 3) {
        if (::getenv("NUMSTAGES")) throw hoytech::error("can't specify both stages and a target");
        target = std::string(argv[2]);
        if (target.size() % 8 != 0 || target.size() > 64) throw hoytech::error("bad size for target: ", target);
        target = hoytech::from_hex(target);
        numStages = target.size() / 4;
        std::reverse(target.begin(), target.end());

        if (target.size() != numStages * 4) {
            throw hoytech::error("With ", numStages, " stages, target must be ", (numStages * 8), " hex digits");
        }
    }

    target += std::string(32 - target.size(), '\0');


    auto param = [&](const char *name, size_t *p, size_t min = 0, size_t max = std::numeric_limits<uint64_t>::max()) {
        if (::getenv(name)) *p = std::stoul(getenv(name));
        if (numStages < min) throw hoytech::error("param ", name, " too small. min = ", min);
        if (numStages > max) throw hoytech::error("param ", name, " too big. max = ", max);
        std::cout << name << " = " << *p << std::endl;
    };

    param("NUMSTAGES", &numStages, 1, 8);
    param("BATCHSIZE", &batchSize);
    param("MERGELIMIT", &mergeLimit);


    std::string memDir(argv[1]);
    Generator g(memDir, batchSize, mergeLimit);


    size_t currStage = 0;
    while (currStage < (numStages-1) && target.substr(currStage * 4, 4) == std::string(4, '\0')) currStage++;
    size_t currOffset = currStage * 4;

    Elem currTarget;
    currTarget.setToZero();
    memcpy(currTarget.buf + currOffset, target.data() + currOffset, 32 - currOffset);

    Elem accum;
    accum.setToZero();

    while (1) {
        std::cout << "========================" << currStage << std::endl;
        std::cout << "Processing stages 0 - " << currStage << std::endl;
        std::cout << "Target: " << currTarget.hex() << std::endl;

        auto seeds = g.run(currStage + 1, currTarget.buf);

        for (auto seed : seeds) {
            uint8_t hash[32];
            std::string buf = std::to_string(seed);
            SHA256(reinterpret_cast<unsigned char*>(buf.data()), buf.size(), reinterpret_cast<unsigned char*>(hash));
            std::cout << hoytech::to_hex(std::string_view(reinterpret_cast<const char*>(hash), 32)) << " (" << seed << ")" << std::endl;
            std::reverse(hash, hash + 32);
            accum.add(hash);
        }

        currStage++;
        if (currStage >= numStages) break;
        currOffset = currStage * 4;

        // Remove dummy final stage and actual final stage
        g.stages.pop_back();
        g.stages.pop_back();
        if (g.stages.size()) g.stages.back().found->clear(); // not reachable anymore

        Elem a1, a2;
        a1.setToZero();
        a2.setToZero();
        memcpy(a1.buf, accum.buf + currOffset, 4);
        memcpy(a2.buf, target.data() + currOffset, 4);
        a1.negateAndAdd(a2.buf);

        currTarget.setToZero();
        memcpy(currTarget.buf + currOffset, a1.buf, 4);
        memcpy(currTarget.buf + currOffset + 4, target.data() + currOffset + 4, 32 - (currOffset + 4));
    }

    return 0;
}
