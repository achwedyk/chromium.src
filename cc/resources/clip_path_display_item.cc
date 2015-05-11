// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/clip_path_display_item.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

ClipPathDisplayItem::ClipPathDisplayItem() {
}

ClipPathDisplayItem::~ClipPathDisplayItem() {
}

void ClipPathDisplayItem::SetNew(const SkPath& clip_path,
                                 SkRegion::Op clip_op,
                                 bool antialias) {
  clip_path_ = clip_path;
  clip_op_ = clip_op;
  antialias_ = antialias;

  size_t memory_usage = sizeof(SkPath) + sizeof(SkRegion::Op) + sizeof(bool);
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 1 /* op_count */,
                      memory_usage);
}

void ClipPathDisplayItem::Raster(SkCanvas* canvas,
                                 SkDrawPictureCallback* callback) const {
  canvas->save();
  canvas->clipPath(clip_path_, clip_op_, antialias_);
}

void ClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString(base::StringPrintf("ClipPathDisplayItem length: %d",
                                         clip_path_.countPoints()));
}

EndClipPathDisplayItem::EndClipPathDisplayItem() {
  DisplayItem::SetNew(true /* suitable_for_gpu_raster */, 0 /* op_count */,
                      0 /* memory_usage */);
}

EndClipPathDisplayItem::~EndClipPathDisplayItem() {
}

void EndClipPathDisplayItem::Raster(SkCanvas* canvas,
                                    SkDrawPictureCallback* callback) const {
  canvas->restore();
}

void EndClipPathDisplayItem::AsValueInto(
    base::trace_event::TracedValue* array) const {
  array->AppendString("EndClipPathDisplayItem");
}

}  // namespace cc
