#include <gtest/gtest.h>

extern "C" {
#include "store.h"
};

struct TestNodeData {
    std::string key;
    std::string value;
};

static const void* get_key(const void* data){
    return static_cast<const TestNodeData*>(data)->key.c_str();
}

static int cmp(const void* l, const void* r) {
    return strcmp((const char*)l, (const char*)r);
}

static void on_extend (pec_hashtable_t* tbl, void* ext_data) {

}

static void on_insert (struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/) {

}

static void node_destroy(void* data) {
    delete static_cast<TestNodeData*>(data);
}

static pec_hashtable_operation_t ops {
    .hash = default_string_hash,
    .get_key = get_key,
    .key_cmp = cmp,
    .on_extend_start = on_extend,
    .on_extend_end = on_extend,
    .on_insert_start = on_insert,
    .on_insert_end = on_insert,
    .on_node_destroy = node_destroy
};

TEST(pec_hashtable__test, init) {
    pec_hashtable_t tbl;
    pec_hashtable_init(&tbl, 11, 1, 7, &ops);
    auto node = new TestNodeData{"key1", "1"};
    pec_hashtable_inset(&tbl, node, NULL);
    pec_hashtable_destroy(&tbl);
}

TEST(pec_hashtable__test, insert_and_search) {
    pec_hashtable_t tbl;
    std::string keys[] = {"key1", "key4dsf", "key4sadasdsf", "kdasey4dsf", "kdeys4dsf", "kfasy4dsfsa"};
    pec_hashtable_init(&tbl, 11, 1, 7, &ops);
    auto node = new TestNodeData{keys[0], "1"};
    pec_hashtable_inset(&tbl, node, NULL);
    node = new TestNodeData{keys[1], "3"};
    pec_hashtable_inset(&tbl, node, NULL);
    node = new TestNodeData{keys[2], "5"};
    pec_hashtable_inset(&tbl, node, NULL);
    node = new TestNodeData{keys[3], "1"};
    pec_hashtable_inset(&tbl, node, NULL);
    node = new TestNodeData{keys[4], "10"};
    pec_hashtable_inset(&tbl, node, NULL);
    node = new TestNodeData{keys[5], "86"};
    pec_hashtable_inset(&tbl, node, NULL);

    pec_hashtable_destroy(&tbl);
}

//void (*on_insert_start)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
//void (*on_insert_end)(struct pec_hashtable* /*table */, size_t /*index*/, void* /* external data*/);
//void (*on_node_destroy)(void* /* node data */);