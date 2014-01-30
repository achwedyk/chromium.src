// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for eliding and formatting UI text.

#ifndef UI_GFX_TEXT_ELIDER_H_
#define UI_GFX_TEXT_ELIDER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/gfx/gfx_export.h"

class GURL;

namespace base {
class FilePath;
}

namespace gfx {
class FontList;

GFX_EXPORT extern const char kEllipsis[];
GFX_EXPORT extern const base::char16 kEllipsisUTF16[];
GFX_EXPORT extern const base::char16 kForwardSlash;


// Helper class to split + elide text, while respecting UTF16 surrogate pairs.
class StringSlicer {
 public:
  StringSlicer(const base::string16& text,
               const base::string16& ellipsis,
               bool elide_in_middle);

  // Cuts |text_| to be |length| characters long. If |elide_in_middle_| is true,
  // the middle of the string is removed to leave equal-length pieces from the
  // beginning and end of the string; otherwise, the end of the string is
  // removed and only the beginning remains. If |insert_ellipsis| is true,
  // then an ellipsis character will be inserted at the cut point.
  base::string16 CutString(size_t length, bool insert_ellipsis);

 private:
  // Returns a valid cut boundary at or before/after |index|.
  size_t FindValidBoundaryBefore(size_t index) const;
  size_t FindValidBoundaryAfter(size_t index) const;

  // The text to be sliced.
  const base::string16& text_;

  // Ellipsis string to use.
  const base::string16& ellipsis_;

  // If true, the middle of the string will be elided.
  bool elide_in_middle_;

  DISALLOW_COPY_AND_ASSIGN(StringSlicer);
};

// Elides a well-formed email address (e.g. username@domain.com) to fit into
// |available_pixel_width| using the specified |font_list|.
// This function guarantees that the string returned will contain at least one
// character, other than the ellipses, on either side of the '@'. If it is
// impossible to achieve these requirements: only an ellipsis will be returned.
// If possible: this elides only the username portion of the |email|. Otherwise,
// the domain is elided in the middle so that it splits the available width
// equally with the elided username (should the username be short enough that it
// doesn't need half the available width: the elided domain will occupy that
// extra width).
GFX_EXPORT base::string16 ElideEmail(const base::string16& email,
                                     const gfx::FontList& font_list,
                                     float available_pixel_width);

enum ElideBehavior {
  // Add ellipsis at the end of the string.
  ELIDE_AT_END,
  // Add ellipsis in the middle of the string.
  ELIDE_IN_MIDDLE,
  // Truncate the end of the string.
  TRUNCATE_AT_END,
  // No eliding of the string.
  NO_ELIDE
};

// Elides |text| to fit in |available_pixel_width| according to the specified
// |elide_behavior|.
GFX_EXPORT base::string16 ElideText(const base::string16& text,
                                    const gfx::FontList& font_list,
                                    float available_pixel_width,
                                    ElideBehavior elide_behavior);

// Elide a filename to fit a given pixel width, with an emphasis on not hiding
// the extension unless we have to. If filename contains a path, the path will
// be removed if filename doesn't fit into available_pixel_width. The elided
// filename is forced to have LTR directionality, which means that in RTL UI
// the elided filename is wrapped with LRE (Left-To-Right Embedding) mark and
// PDF (Pop Directional Formatting) mark.
GFX_EXPORT base::string16 ElideFilename(const base::FilePath& filename,
                                        const gfx::FontList& font_list,
                                        float available_pixel_width);

// Functions to elide strings when the font information is unknown.  As
// opposed to the above functions, the ElideString() and
// ElideRectangleString() functions operate in terms of character units,
// not pixels.

// If the size of |input| is more than |max_len|, this function returns
// true and |input| is shortened into |output| by removing chars in the
// middle (they are replaced with up to 3 dots, as size permits).
// Ex: ElideString(ASCIIToUTF16("Hello"), 10, &str) puts Hello in str and
// returns false.  ElideString(ASCIIToUTF16("Hello my name is Tom"), 10, &str)
// puts "Hell...Tom" in str and returns true.
// TODO(tsepez): Doesn't handle UTF-16 surrogate pairs properly.
// TODO(tsepez): Doesn't handle bidi properly.
GFX_EXPORT bool ElideString(const base::string16& input, int max_len,
                            base::string16* output);

// Reformat |input| into |output| so that it fits into a |max_rows| by
// |max_cols| rectangle of characters.  Input newlines are respected, but
// lines that are too long are broken into pieces.  If |strict| is true,
// we break first at naturally occuring whitespace boundaries, otherwise
// we assume some other mechanism will do this in approximately the same
// spot after the fact.  If the word itself is too long, we always break
// intra-word (respecting UTF-16 surrogate pairs) as necssary. Truncation
// (indicated by an added 3 dots) occurs if the result is still too long.
//  Returns true if the input had to be truncated (and not just reformatted).
GFX_EXPORT bool ElideRectangleString(const base::string16& input,
                                     size_t max_rows,
                                     size_t max_cols,
                                     bool strict,
                                     base::string16* output);

// Specifies the word wrapping behavior of |ElideRectangleText()| when a word
// would exceed the available width.
enum WordWrapBehavior {
  // Words that are too wide will be put on a new line, but will not be
  // truncated or elided.
  IGNORE_LONG_WORDS,

  // Words that are too wide will be put on a new line and will be truncated to
  // the available width.
  TRUNCATE_LONG_WORDS,

  // Words that are too wide will be put on a new line and will be elided to the
  // available width.
  ELIDE_LONG_WORDS,

  // Words that are too wide will be put on a new line and will be wrapped over
  // multiple lines.
  WRAP_LONG_WORDS,
};

// Indicates whether the |available_pixel_width| by |available_pixel_height|
// rectangle passed to |ElideRectangleText()| had insufficient space to
// accommodate the given |text|, leading to elision or truncation.
enum ReformattingResultFlags {
  INSUFFICIENT_SPACE_HORIZONTAL = 1 << 0,
  INSUFFICIENT_SPACE_VERTICAL = 1 << 1,
};

// Reformats |text| into output vector |lines| so that the resulting text fits
// into an |available_pixel_width| by |available_pixel_height| rectangle with
// the specified |font_list|. Input newlines are respected, but lines that are
// too long are broken into pieces. For words that are too wide to fit on a
// single line, the wrapping behavior can be specified with the |wrap_behavior|
// param. Returns a combination of |ReformattingResultFlags| that indicate
// whether the given rectangle had insufficient space to accommodate |texŧ|,
// leading to elision or truncation (and not just reformatting).
GFX_EXPORT int ElideRectangleText(const base::string16& text,
                                  const gfx::FontList& font_list,
                                  float available_pixel_width,
                                  int available_pixel_height,
                                  WordWrapBehavior wrap_behavior,
                                  std::vector<base::string16>* lines);

// Truncates the string to length characters. This breaks the string at
// the first word break before length, adding the horizontal ellipsis
// character (unicode character 0x2026) to render ...
// The supplied string is returned if the string has length characters or
// less.
GFX_EXPORT base::string16 TruncateString(const base::string16& string,
                                         size_t length);

}  // namespace gfx

#endif  // UI_GFX_TEXT_ELIDER_H_
