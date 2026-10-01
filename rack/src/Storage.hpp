// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../lib/core_engine/card_format.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ecto {
namespace fs=std::filesystem;
template<class T,size_t N> class Queue {
    std::array<T,N> values{};std::atomic<size_t> head{0},tail{0};
public:
    bool canPush() const {return (head.load(std::memory_order_relaxed)+1)%N!=tail.load(std::memory_order_acquire);}
    bool push(const T &value) {auto h=head.load(std::memory_order_relaxed),n=(h+1)%N;if(n==tail.load(std::memory_order_acquire))return false;values[h]=value;head.store(n,std::memory_order_release);return true;}
    bool pop(T &value) {auto t=tail.load(std::memory_order_relaxed);if(t==head.load(std::memory_order_acquire))return false;value=values[t];tail.store((t+1)%N,std::memory_order_release);return true;}
};
struct Wav {uint64_t offset=0,bytes=0;unsigned rate=0,channels=0;};
struct Entry {
    CoreCardInfo info{};std::array<Wav,2> wav;bool companion=false;std::string name;
};
struct Library {
    fs::path root;uint64_t generation=0;
    std::array<std::array<std::unique_ptr<Entry>,16>,16> entries;
    std::array<CoreBank,16> banks{};
    std::map<std::string,std::string> settings;
    CoreState preparedSettings{};
    bool runePresent[7]{};bool importSettings=false;
    std::vector<std::string> warnings;
};
struct Bank {
    std::shared_ptr<Library> library;unsigned index=0;
    std::array<std::vector<uint8_t>,16> primary;
    uint64_t bytes=0;
};
struct Request {uint64_t generation=0;uint32_t page=0;uint8_t bank=0,slot=0;};
class Storage {
public:
    static constexpr size_t pageBytes=65536,pageCount=1024;
    Storage();~Storage();
    Storage(const Storage&)=delete;Storage &operator=(const Storage&)=delete;
    // UI only; engine bank changes use the atomic mailbox below.
    void load(const std::string &root,unsigned bank,bool importSettings=false);
    std::string status();
    std::atomic<int> requestedBank{-1};
    std::atomic<uint64_t> activeGeneration{0};
    std::atomic<uint64_t> misses{0};
    Queue<Bank*,8> ready;
    Queue<Bank*,32> retired;
    bool read(Bank *,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *,size_t);
    static std::shared_ptr<Library> catalogue(const fs::path &,uint64_t);
    static std::unique_ptr<Bank> prepare(std::shared_ptr<Library>,unsigned);
    static Wav inspect(const fs::path &);
    static fs::path path(const Library&,unsigned,unsigned,unsigned);
private:
    struct Page {
        // 0 empty, 1 published, 2 audio-reader pin, 3 worker writing.
        std::atomic<unsigned> state{0};Request key;size_t size=0;
        std::array<uint8_t,pageBytes> data{};
    };
    std::unique_ptr<Page[]> pages;
    Queue<Request,4096> requests;
    std::thread worker;std::atomic<bool> stop{false};
    std::mutex mutex;std::string message="Choose a sample folder",pendingRoot;
    unsigned pendingBank=0;bool pendingImport=false;uint64_t revision=0;
    void run();void fill(const Request &,const Library &);
    bool cached(const Request &,size_t,void *,size_t);
    static size_t hash(const Request &);
};
void applySettings(const Library&,CoreState&);
}
