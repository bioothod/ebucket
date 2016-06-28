package ebucket

/*
#cgo LDFLAGS: -lelliptics_cpp -lelliptics_client -lpthread -lmsgpack -lboost_thread -lboost_system
#cgo CXXFLAGS: -std=c++0x -W -Wall -I../../include

#include "ebucket.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char**makeCharArray(int size) {
        return calloc(sizeof(char*), size);
}

static void setArrayString(char **a, char *s, int n) {
        a[n] = s;
}

static void freeCharArray(char **a, int size) {
        int i;
        for (i = 0; i < size; i++)
                free(a[i]);
        free(a);
}
*/
import "C"

import (
	"fmt"
	"github.com/bioothod/elliptics-go/elliptics"
	"unsafe"
)

type BucketMeta struct {
	Name		string
	Groups		[]uint32
}

type BucketProcessor struct {
	node		*elliptics.Node
	bp		unsafe.Pointer
}

func NewBucketProcessorKey(node *elliptics.Node, groups []uint32, key string) (*BucketProcessor, error) {
	ckey := C.CString(key)
	defer C.free(unsafe.Pointer(ckey))

	cnode := node.GetRawPointer()
	bp := C.new_ebucket_wrapper_key(cnode, (*C.uint32_t)(&groups[0]), C.int(len(groups)), ckey)
	if bp == nil {
		return nil, fmt.Errorf("could not create ebucket wrapper, check stderr output")
	}

	return &BucketProcessor {
		node:		node,
		bp:		bp,
	}, nil
}


func NewBucketProcessor(node *elliptics.Node, groups []uint32, bnames []string) (*BucketProcessor, error) {
	cnames := C.makeCharArray(C.int(len(bnames)))
	defer C.freeCharArray(cnames, C.int(len(bnames)))
	for i, s := range bnames {
		C.setArrayString(cnames, C.CString(s), C.int(i))
	}

	cnode := node.GetRawPointer()
	bp := C.new_ebucket_wrapper(cnode, (*C.uint32_t)(&groups[0]), C.int(len(groups)), cnames, C.int(len(bnames)))
	if bp == nil {
		return nil, fmt.Errorf("could not create ebucket wrapper, check stderr output")
	}

	return &BucketProcessor {
		node:		node,
		bp:		bp,
	}, nil
}

func (bp *BucketProcessor) Close() {
	C.delete_ebucket_wrapper(bp.bp)
}

func (bp *BucketProcessor) GetBucket(size uint64) (*BucketMeta, error) {
	var meta C.bucket_meta
	err := C.ebucket_get_bucket(bp.bp, C.size_t(size), unsafe.Pointer(&meta))
	if err != 0 {
		return nil, fmt.Errorf("could not get bucket for size: %d, error: %d", size, err)
	}

	bm := &BucketMeta {
		Name:		C.GoStringN(C.bucket_meta_name(unsafe.Pointer(&meta)),
					C.int(C.bucket_meta_name_size(unsafe.Pointer(&meta)))),
		Groups:		make([]uint32, C.bucket_meta_groups_size(unsafe.Pointer(&meta))),
	}
	C.bucket_meta_groups_copy(unsafe.Pointer(&bm.Groups[0]), unsafe.Pointer(&meta));

	return bm, nil
}

func (bp *BucketProcessor) FindBucket(name string) (*BucketMeta, error) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	var meta C.bucket_meta
	err := C.ebucket_find_bucket(bp.bp, cname, unsafe.Pointer(&meta))
	if err != 0 {
		return nil, fmt.Errorf("could not find bucket with name: %s, error: %d", name, err)
	}

	bm := &BucketMeta {
		Name:		C.GoStringN(C.bucket_meta_name(unsafe.Pointer(&meta)),
					C.int(C.bucket_meta_name_size(unsafe.Pointer(&meta)))),
		Groups:		make([]uint32, C.bucket_meta_groups_size(unsafe.Pointer(&meta))),
	}
	C.bucket_meta_groups_copy(unsafe.Pointer(&bm.Groups[0]), unsafe.Pointer(&meta));

	return bm, nil
}
