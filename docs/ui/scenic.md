# Scenic, the Fuchsia graphics engine

* [Introduction](#introduction)
* [Concepts](#concepts)
  * [Scenic](#scenic)
  * [Sessions](#sessions)
  * [Resources](#resources)
    * [Nodes](#nodes)
    * [Scenes](#scenes)
    * [Compositors](#compositors)
  * [Timing Model](#timing-model)
  * [Fences](#fences)
* [API Guide](#api-guide)
  * [FIDL interfaces](#fidl-interfaces)

# Introduction

Scenic is a Garnet service that composes graphical objects from multiple
processes into a shared scene graph.  These objects are rendered within a
unified lighting environment (to a display or other render target); this
means that the objects can cast shadows or reflect light onto each other,
even if the originating processes have no knowledge of each other.

Scenic's responsibilities are:

- Composition: Scenic provides a retained-mode 3D scene graph that contains
  content that is independently generated and linked together by its
  clients.  Composition makes it possible to seamlessly intermix
  the graphical content of separately implemented UI components.

- Animation: Scenic re-evaluates any time-varying expressions within models
  prior to rendering each frame, thereby enabling animation of model
  properties without further intervention from clients.  Offloading
  animations to Scenic ensures smooth jank-free transitions.

- Rendering: Scenic renders its scene graph using Escher, a rendering
  library built on Vulkan, which applies realistic lighting and shadows to
  the entire scene.

- Scheduling: Scenic schedules scene updates and animations to anticipate
  and match the rendering target's presentation latency and refresh interval.

- Diagnostics: Scenic provides a diagnostic interface to help developers
  debug their models and measure performance.

# Concepts

## Scenic

The `Scenic` FIDL interface is Scenic's front door.  Each instance of the
interface represents a Scenic instance. Each Scenic instance is an isolated
rendering context with its own content, render targets, and scheduling loop.

The `Scenic` interface allows a client to create a [`Session`](#session) which
is the communication channel used to publish graphical content to this instance.

A single Scenic instance can update, animate, and render multiple
`Scenes` (trees of graphical objects) to multiple targets in tandem on the same
scheduling loop.  This means that the timing model for a Scenic instance
is _coherent_: all of its associated content belongs to the same scheduling
domain and can be seamlessly intermixed.

In practice, there is one instance of Scenic and one Scene that is rendered to a
target. However, creating separate Scenic instances can be useful for rendering
to targets which have very different scheduling requirements or for running
tests in isolation. Independent Scenic instances cannot share content and are
therefore not coherent amongst themselves.

When a Scenic instance is destroyed, all of its sessions become
inoperable and its rendering ceases.

`Views` typically do not deal with the Scenic instance directly; instead
they receive a Scenic `Session` from the view manager.

## Sessions

The `Session` FIDL interface is the primary API used by clients of Scenic to
contribute graphical content in the form of `Resources`.  Each session has
its own resource table and is unable to directly interact with resources
belonging to other sessions.

Each session provides the following operations:

- Submit operations to add, remove, or modify resources.
- Commit a sequence of operations to be presented atomically.
- Awaiting and signaling fences.
- Schedule subsequent frame updates.
- Form links with other sessions (by mutual agreement).

When a session is destroyed, all of its resources are released and all of
its links become inoperable.

`Views` typically receive separate sessions from the view manager.

## Resources

`Resources` represent scene elements such as nodes, shapes, materials, and
animations which belong to particular `Sessions`.

The list of Scenic resources is described by the API:
//garnet/public/fidl/fuchsia.ui.gfx/resources.fidl

Clients of Scenic generate graphical content to be rendered by queuing and
submitting operations to add, remove, or modify resources within their
session.

Each resource is identified within its session by a locally unique id which
is assigned by the owner of the session (by arbitrary means).  Sessions
cannot directly refer to resources which belong to other sessions (even if
they happen to know their id) therefore content embedding between sessions
is performed using `Link` objects as intermediaries.

To add a resource, perform the following steps:

- Enqueue an operation to add a resource of the desired type and assign it a
  locally unique id within the session.
- Enqueue one or more operations to set that resource's properties given its
  id.

Certain more complex resources may reference the ids of other resources
within their own definition.  For instance, a `Node` references its `Shape`
thus the `Shape` must be added before the `Node` so that the node may
reference it as part of its definition.

To modify a resource, enqueue one or more operations to set the desired
properties in the same manner used when the resource was added.

The remove a resource, enqueue an operation to remove the resource.

Removing a resource causes its id to become available for reuse.  However,
the session maintains a reference count for each resource which is
internally referenced.  The underlying storage will not be released (and
cannot be reused) until all remaining references to the resource have been
cleared *and* until the next frame which does not require the resource has
been presented.  This is especially important for `Memory` resources.
See also [Fences](#fences).

This process of addition, modification, and removal may be repeated
indefinitely to incrementally update resources within a session.

### Nodes

A `Node` resource represents a graphical object which can be assembled into
a hierarchy called a `node tree` for rendering.

TODO: Discuss this in more detail, especially hierarchical modeling concepts
such as per-node transforms, groups, adding and removing children, etc.

### Scenes

A `Scene` resource combines a tree of nodes with the scene-wide parameters
needed to render it.  A Scenic instance may contain multiple scenes but
each scene must have its own independent tree of nodes.

A scene resource has the following properties:

- The scene's root node.
- The scene's global parameters such as its lighting model.

In order to render a scene, a `Camera` must be pointed at it.

### Compositors

Compositors are resources that come in two flavors: `DisplayCompositor` and
`ImagePipeCompositor`; their job is to draw the content of a `LayerStack`
into their render target.  For `DisplayCompositor`, the target display may
have multiple hardware overlays; in this case the compositor may choose
associate each of these with a separate layer, rather than flattening the
layers into a single image.

A `LayerStack` resource consists of an ordered list of `Layers`.  Each layer
can contain either an `Image` (perhaps transformed by a matrix), or a
`Camera` that points at a `Scene` to be rendered (as described above).

### TODO: More Resources

Add sections to discuss all other kinds of resources: shapes, materials,
links, memory, images, buffers, animations, variables, renderers etc.

## Timing Model

TODO: Talk about scheduling frames, presentation timestamps, etc.

## Fences

TODO: Talk about synchronization.

# API Guide

## FIDL interfaces

The following files define and document the collection of FIDL interfaces that
make up Scenic.

* [Scenic top-level interfaces](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.scenic) (`fuchsia.ui.scenic`)
  * [scenic.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.scenic/scenic.fidl)
  * [session.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.scenic/session.fidl)
  * [commands.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.scenic/commands.fidl)
  * [events.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.scenic/events.fidl)

* [Gfx](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx) (`fuchsia.ui.gfx`)
  * [commands.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx/commands.fidl)
  * [events.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx/events.fidl)
  * [resources.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx/resources.fidl)
  * [nodes.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx/nodes.fidl)
  * [shapes.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx/shades.fidl)
  * [...](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.gfx)

* [Views](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.views) (`fuchsia.ui.views`)
  * [commands.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.views/commands.fidl)
  * [events.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.views/events.fidl)

* [Input](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.input) (`fuchsia.ui.input`)
  * [commands.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.input/commands.fidl)
  * [input_events.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.input/input_events.fidl)

* [Policy](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.policy) (`fuchsia.ui.policy`)
  * [presenter.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.policy/presenter.fidl)
  * [presentation.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.policy/presentation.fidl)
  * [...](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.policy)

* [App](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.app) (`fuchsia.ui.app`)
  * [view_provider.fidl](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.app/view_provider.fidl)

* [experimental] [Sketchy](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.sketchy) (`fuchsia.ui.sketchy`)
* [experimental] [Vectorial](https://fuchsia.googlesource.com/garnet/+/master/public/fidl/fuchsia.ui.vectorial) (`fuchsia.ui.vectorial`)

## TODO

Talk about how to get started using Scenic, running examples,
recommended implementation strategies, etc.
