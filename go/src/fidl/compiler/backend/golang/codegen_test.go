// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package golang

import (
	"strings"
	"testing"

	"fidl/compiler/backend/golang/ir"
	"fidl/compiler/backend/typestest"

	"github.com/google/go-cmp/cmp"
)

var cases = map[string]string{
	"doc_comments.fidl.json": `
// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING: This file is machine generated by fidlgen.

package name


import (
	_zx "syscall/zx"
	_bindings "syscall/zx/fidl"
)



const (
	// const comment #1
	//
	// const comment #3
	C int32 = 4
)



// struct comment #1
//
// struct comment #3
type Struct struct {
	_ struct{} ` + "`" + `fidl2:"s,4,4"` + "`" + `
	// struct member comment #1
	//
	// struct member comment #3
	Field int32 
}

var _mStruct = _bindings.CreateLazyMarshaler(Struct{})

func (msg *Struct) Marshaler() _bindings.Marshaler {
	return _mStruct
}

// Implements Payload.
func (_ *Struct) InlineAlignment() int {
	return 4
}

// Implements Payload.
func (_ *Struct) InlineSize() int {
	return 4
}
type UnionTag uint32
const (
	_ UnionTag = iota
	UnionField
)


// union comment #1
//
// union comment #3
type Union struct {
	UnionTag ` + "`" + `fidl:"tag" fidl2:"u,8,4"` + "`" + `
	// union member comment #1
	//
	// union member comment #3
	Field int32 
}

// Implements Payload.
func (_ *Union) InlineAlignment() int {
	return 4
}

// Implements Payload.
func (_ *Union) InlineSize() int {
	return 8
}

func (u *Union) Which() UnionTag {
	return u.UnionTag
}

func (u *Union) SetField(field int32) {
	u.UnionTag = UnionField
	u.Field = field
}
const (
	InterfaceMethodOrdinal uint32 = 1
)
// Request for Method.

type InterfaceMethodRequest struct {
	_ struct{} ` + "`" + `fidl2:"s,0,0"` + "`" + `
}

var _mInterfaceMethodRequest = _bindings.CreateLazyMarshaler(InterfaceMethodRequest{})

func (msg *InterfaceMethodRequest) Marshaler() _bindings.Marshaler {
	return _mInterfaceMethodRequest
}

// Implements Payload.
func (_ *InterfaceMethodRequest) InlineAlignment() int {
	return 0
}

// Implements Payload.
func (_ *InterfaceMethodRequest) InlineSize() int {
	return 0
}
// Method has no response.

type InterfaceInterface _bindings.Proxy

func (p *InterfaceInterface) Method() error {
	req_ := InterfaceMethodRequest{
	}
	err := ((*_bindings.Proxy)(p)).Send(InterfaceMethodOrdinal, &req_)
	return err
}


// interface comment #1
//
// interface comment #3
type Interface interface {
	// method comment #1
	//
	// method comment #3
	Method() error
}

type InterfaceInterfaceRequest _bindings.InterfaceRequest

func NewInterfaceInterfaceRequest() (InterfaceInterfaceRequest, *InterfaceInterface, error) {
	req, cli, err := _bindings.NewInterfaceRequest()
	return InterfaceInterfaceRequest(req), (*InterfaceInterface)(cli), err
}

type InterfaceStub struct {
	Impl Interface
}

func (s *InterfaceStub) Dispatch(ord uint32, b_ []byte, h_ []_zx.Handle) (_bindings.Payload, error) {
	switch ord {
	case InterfaceMethodOrdinal:
		in_ := InterfaceMethodRequest{}
		if err_ := _bindings.Unmarshal(b_, h_, &in_); err_ != nil {
			return nil, err_
		}
		err_ := s.Impl.Method()
		return nil, err_
	}
	return nil, _bindings.ErrUnknownOrdinal
}

type InterfaceService struct {
	_bindings.BindingSet
}

func (s *InterfaceService) Add(impl Interface, c _zx.Channel, onError func(error)) (_bindings.BindingKey, error) {
	return s.BindingSet.Add(&InterfaceStub{Impl: impl}, c, onError)
}

func (s *InterfaceService) EventProxyFor(key _bindings.BindingKey) (*InterfaceEventProxy, bool) {
	pxy, err := s.BindingSet.ProxyFor(key)
	return (*InterfaceEventProxy)(pxy), err
}

type InterfaceEventProxy _bindings.Proxy
`}

func TestCodegen(t *testing.T) {
	for filename, expected := range cases {
		t.Run(filename, func(t *testing.T) {
			fidl := typestest.GetExample(filename)
			tree := ir.Compile(fidl)

			implDotGo, err := NewFidlGenerator().GenerateImplDotGo(tree)
			if err != nil {
				t.Fatalf("unexpected error while generating impl.go: %s", err)
			}

			var (
				splitExpected  = strings.Split(strings.TrimSpace(expected), "\n")
				splitImplDotGo = strings.Split(strings.TrimSpace(string(implDotGo)), "\n")
			)
			if diff := cmp.Diff(splitExpected, splitImplDotGo); diff != "" {
				t.Errorf("unexpected impl.go: %s\ngot: %s", diff, string(implDotGo))
			}
		})
	}
}