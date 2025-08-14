// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cheriot/cheriot_register.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "mpact/sim/generic/arch_state.h"
#include "mpact/sim/generic/register.h"

namespace mpact {
namespace sim {
namespace cheriot {

using PermissionFormats = CheriotRegister::PermissionFormats;

// Helper function to extract a bit range from a uint64_t given msb and lsb.
template <typename T>
static inline T Extract(T value, int msb, int lsb) {
  T shift = lsb;
  T mask = (1ULL << (msb - lsb + 1)) - 1;
  return (value >> shift) & mask;
}

CheriotRegister::CheriotRegister(generic::ArchState* state,
                                 absl::string_view name)
    : generic::Register<uint32_t>(state, name) {
  ResetNull();
}

void CheriotRegister::ResetNull() {
  // When called by the constructor, the data buffer may be nullptr.
  if (data_buffer() != nullptr) {
    data_buffer()->Set<uint32_t>(0, 0);
  }
  Clear();
}

void CheriotRegister::ResetMemoryRoot() {
  set_base(0);
  set_top(0x1'0000'0000ULL);
  set_permissions_format(kMemoryCapReadWrite);
  set_permissions(kPermitGlobal | kPermitLoad | kPermitStore |
                  kPermitLoadStoreCapability | kPermitStoreLocalCapability |
                  kPermitLoadGlobal | kPermitLoadMutable);
  set_object_type(kUnsealed);
  set_reserved(0);
  set_tag(true);
  is_dirty_ = true;
  is_null_ = false;
  exponent_ = 24;
  raw_ = Compress();
}

void CheriotRegister::ResetExecuteRoot() {
  set_base(0);
  set_top(0x1'0000'0000ULL);
  set_permissions_format(kExecutable);
  set_permissions(kPermitGlobal | kPermitExecute | kPermitLoad |
                  kPermitLoadStoreCapability | kPermitLoadGlobal |
                  kPermitLoadMutable | kPermitAccessSystemRegisters);
  set_object_type(kUnsealed);
  set_reserved(0);
  set_tag(true);
  is_dirty_ = true;
  is_null_ = false;
  exponent_ = 24;
  raw_ = Compress();
}

void CheriotRegister::ResetSealingRoot() {
  set_base(0);
  set_top(0x1'0000'0000ULL);
  set_permissions_format(kSealing);
  set_permissions(kPermitGlobal | kPermitSeal | kPermitUnseal | kUserPerm0);
  set_object_type(kUnsealed);
  set_reserved(0);
  set_tag(true);
  is_dirty_ = true;
  is_null_ = false;
  exponent_ = 24;
  raw_ = Compress();
}

void CheriotRegister::Clear() {
  set_base(0);
  set_top(0);
  set_permissions_format(kSealing);
  set_permissions(0);
  set_object_type(kUnsealed);
  set_reserved(0);
  set_tag(false);
  is_dirty_ = true;
  is_null_ = false;
  raw_ = 0;
  exponent_ = 0;
}

void CheriotRegister::ClearPermissions(uint32_t permission_bits) {
  set_permissions(permissions() & ~permission_bits);
  set_permissions(ExpandPermissions(CompressPermissions()));
}

bool CheriotRegister::SetBounds(uint32_t req_base, uint64_t req_length) {
  if (is_null_) {
    Expand(address(), 0, /*tag=*/false);
    is_null_ = false;
  }
  // Compute the requested top based on base and length.
  uint64_t new_top = static_cast<uint64_t>(req_base) + req_length;
  uint32_t exp;
  // Determine the appropriate exponent in order to perform proper rounding
  // of base and top.
  if (req_length >= 0x1'0000'0000ULL) {
    exp = 24;
  } else {
    exp = 23 - absl::countl_zero(static_cast<uint32_t>(req_length) | 0x1ff);
    if (exp > 14) exp = 24;
  }
  if (exp == 0) {
    set_base(req_base);
    set_top(req_base + req_length);
    exponent_ = 0;
    raw_ = Compress();
    return true; /* exact */
  }

  // Adjust base and top as needed.
  uint64_t exp_mask = (1ULL << exp) - 1;
  uint64_t new_base = req_base & ~exp_mask;
  uint64_t new_top_correction =
      static_cast<uint64_t>((new_top & exp_mask) != 0);
  new_top &= ~exp_mask;
  // Correct for any truncation in top if top was rounded down.
  new_top += new_top_correction << exp;
  // Recompute the length based on the rounded base and top.
  uint64_t new_length = new_top - new_base;
  uint32_t new_exp;
  if (new_length >= 0x1'0000'0000ULL) {
    new_exp = 24;
  } else {
    new_exp = 23 - absl::countl_zero(static_cast<uint32_t>(new_length) | 0x1ff);
    if (new_exp > 14) new_exp = 24;
  }
  exponent_ = new_exp;
  // Check if the rounding of base and top increased the length so much that it
  // now requires a larger exponent. If so, recompute base and top. This can
  // only happen once, so no need to recheck.
  if (new_exp > exp) {
    new_top = static_cast<uint64_t>(new_base) + new_length;
    if (tag() && (new_top > 0x1'0000'0000ULL)) new_top = 0x1'0000'0000ULL;
    exp_mask = (1ULL << new_exp) - 1;
    // Adjust base and top as needed.
    new_base &= ~exp_mask;
    new_top_correction = static_cast<uint64_t>((new_top & exp_mask) != 0);
    new_top &= ~exp_mask;
    // Correct for any truncation.
    new_top += new_top_correction << exp;
  }
  // Set the top and base.
  set_top(new_top);
  set_base(new_base);
  raw_ = Compress();
  // If the address is not in bounds, clear the tag.
  if ((address() > top()) || (address() < base())) Invalidate();
  // If the length and the base are the same as requested, the bounds were
  // set exactly.
  return (req_base == new_base) && (new_length == req_length);
}

std::pair<uint32_t, uint64_t> CheriotRegister::ComputeBounds() {
  if (is_null_) {
    Expand(address(), 0, /*tag=*/false);
  }
  uint64_t base_bits = raw_ & 0x1ff;
  uint64_t top_bits = (raw_ >> 9) & 0x1ff;
  uint64_t a_mid = (address() >> exponent_) & 0x1ff;
  uint64_t a_hi = (a_mid < base_bits ? 1 : 0);
  uint64_t t_hi = top_bits < base_bits ? 1 : 0;
  uint64_t c_b = 0 - a_hi;
  uint64_t c_t = t_hi - a_hi;
  uint64_t address64 = address();
  uint64_t a_top = address64 >> (exponent_ + 9);
  uint64_t base = ((a_top + c_b) << (exponent_ + 9)) | (base_bits << exponent_);
  base &= 0xffff'ffff;
  uint64_t top = ((a_top + c_t) << (exponent_ + 9)) | (top_bits << exponent_);
  top &= 0x1'ffff'ffffULL;
  return std::make_pair(static_cast<uint32_t>(base), top);
}

uint32_t CheriotRegister::Compress() const {
  if (is_null_) return 0;

  uint32_t compressed = 0;
  // Compute the compressed permissions representation.
  uint32_t permissions_field = CompressPermissions();
  // Only store the low 3 bits of the object type. The fourth is implied based
  // on the capability type.
  compressed |= (object_type() & 0b111) << 22;
  compressed |= permissions_field << 25;
  compressed |= reserved_ << 31;
  // If the expanded capability is not dirty, we don't have to re-do the
  // exponent and top/base computations.
  if (!is_dirty_) {
    compressed |= raw_ & 0x3f'ffff;
    return compressed;
  }
  // Compute exponent.
  uint32_t exp;
  if (length() >= 0x1'0000'0000ULL) {
    exp = 24;
  } else {
    exp = 23 - absl::countl_zero(static_cast<uint32_t>(length()) | 0x1ff);
  }
  uint32_t exp_field = exp;
  // Any exponent over 14 gets set to 24 (the max).
  if (exp > 14) {
    exp = 24;
    exp_field = 0xf;
  }
  // Get the bounds. Note, there is no need for any specific rounding, since
  // bounds are automatically rounded by SetBounds.
  uint32_t top_field = (top() >> exp) & 0x1ff;
  uint32_t base_field = (base() >> exp) & 0x1ff;
  // Assemble the fields.
  compressed |= base_field;
  compressed |= top_field << 9;
  compressed |= exp_field << 18;
  return compressed;
}

void CheriotRegister::Expand(uint32_t address, uint32_t compressed, bool tag) {
  // Extract bit fields.
  uint64_t top_9 = Extract(compressed, kTop[0], kTop[1]);
  uint32_t base_9 = Extract(compressed, kBase[0], kBase[1]);
  uint32_t exp = Extract(compressed, kExponent[0], kExponent[1]);
  if (exp == 15) exp = 24;
  uint64_t a_top = static_cast<uint64_t>(address) >> (exp + 9);
  uint32_t a_mid = Extract(address, exp + 8, exp);

  // Compute correction factors.
  uint64_t a_hi = a_mid < base_9 ? 1 : 0;
  uint64_t t_hi = top_9 < base_9 ? 1 : 0;
  uint64_t c_b = 0 - a_hi;
  uint64_t c_t = t_hi - a_hi;
  uint64_t new_base64 = ((a_top + c_b) << (exp + 9)) | (base_9 << exp);
  uint64_t new_top = ((a_top + c_t) << (exp + 9)) | (top_9 << exp);

  // Only use the lower 32 bits.
  uint64_t new_base = static_cast<uint32_t>(new_base64);
  // Expand permissions.
  uint32_t compressed_permissions =
      Extract(compressed, kPermissions[0], kPermissions[1]);
  uint32_t new_permissions = ExpandPermissions(compressed_permissions);

  // Set the fields.
  if (tag) {
    set_base(0);
    set_top(0x1'0000'0000ULL);
    (void)SetBounds(new_base, new_top - new_base64);
  } else {
    set_base(new_base);
    set_top(new_top & 0x1'ffff'ffffULL);
  }
  exponent_ = exp;
  uint32_t obj_type = Extract(compressed, kObjectType[0], kObjectType[1]);
  if (obj_type && (new_permissions & kPermitExecute) == 0) {
    // Bit 3 of the object type is implied by the capability type. For non
    // execute capabilities, set it to one.
    obj_type |= 0b1'000;
  }
  set_object_type(obj_type);
  set_permissions(ExpandPermissions(compressed_permissions));
  set_reserved(Extract(compressed, kReserved[0], kReserved[1]));
  set_tag(tag);
  data_buffer()->Set<uint32_t>(0, address);
  raw_ = compressed;
  is_dirty_ = false;
  is_null_ = false;
  raw_ = compressed;
}

void CheriotRegister::Validate() {
  if (!tag() | is_null_) return;

  // The capability is still valid if the address is representable.
  set_tag(IsRepresentable());
}

bool CheriotRegister::IsValid() const {
  if (!tag() || is_null_) return false;
  uint32_t address = data_buffer()->Get<uint32_t>(0);
  return (address < top()) && (address >= base());
}

bool CheriotRegister::IsSealed() const {
  if (is_null_ || !tag()) return false;

  if (HasPermission(kPermitExecute)) {
    return (object_type() == kInterruptInheritingSentry) ||
           (object_type() == kInterruptEnablingForwardSentry) ||
           (object_type() == kInterruptDisablingForwardSentry) ||
           (object_type() == kInterruptEnablingBackwardSentry) ||
           (object_type() == kInterruptDisablingBackwardSentry) ||
           (object_type() == kSealedExecutable6) ||
           (object_type() == kSealedExecutable7);
  } else {
    return ((object_type() >= 9) && (object_type() <= 15));
  }
}

absl::Status CheriotRegister::Seal(const CheriotRegister& source,
                                   uint32_t obj_type) {
  if (is_null_) {
    Expand(address(), 0, /*tag=*/false);
    is_null_ = false;
  }
  absl::Status status = absl::OkStatus();
  // Check that the conditions are correct for sealing the target capability.
  if (!tag()) {
    status = absl::InvalidArgumentError("Target is not a valid capability");
  } else if (IsSealed()) {
    status =
        absl::InvalidArgumentError("Cannot seal already sealed capability");
  } else if (!source.tag()) {
    status = absl::InvalidArgumentError(
        "Sealing capability is not a valid capability");
  } else if (source.IsSealed()) {
    status =
        absl::InvalidArgumentError("Cannot seal using a sealed capability");
  } else if ((source.permissions() & PermissionBits::kPermitSeal) == 0) {
    status = absl::PermissionDeniedError("Missing sealing permission");
  } else if (!source.IsValid()) {
    status =
        absl::InvalidArgumentError("Sealing capability address out of range");
  }

  // Different sealing values for memory and execute capabilities.
  if (status.ok()) {
    if (HasPermission(PermissionBits::kPermitExecute)) {
      switch (obj_type) {
        case kInterruptInheritingSentry:
        case kInterruptEnablingForwardSentry:
        case kInterruptDisablingForwardSentry:
        case kInterruptEnablingBackwardSentry:
        case kInterruptDisablingBackwardSentry:
        case kSealedExecutable6:
        case kSealedExecutable7:
          // The sealing type is ok.
          break;
        default:
          // Invalidate if the capability does not have execute permission.
          status = absl::InvalidArgumentError(
              "Invalid object type for executable capability");
          break;
      }
    } else {
      // Invalidate if the object type is out of range.
      if ((obj_type <= 0b1000) || (obj_type > 0b1111)) {
        status = absl::InvalidArgumentError(
            "Invalid object type for non-execute capability");
      }
    }
  }
  // Keep object type 1 wider than the compressed object type.
  obj_type &= (1 << (kObjectType[0] - kObjectType[1] + 1 + 1)) - 1;
  set_object_type(obj_type);
  if (!status.ok()) Invalidate();
  return status;
}

absl::Status CheriotRegister::Unseal(const CheriotRegister& source,
                                     uint32_t obj_type) {
  if (is_null_) {
    Expand(address(), 0, /*tag=*/false);
    is_null_ = false;
  }
  // Check that the conditions are correct for unsealing the target capability.
  auto status = absl::OkStatus();
  if (!tag()) {
    status = absl::InvalidArgumentError("Target is not a valid capability");
  } else if (IsUnsealed()) {
    status =
        absl::InvalidArgumentError("Cannot unseal already unsealed capability");
  } else if (!source.tag()) {
    status = absl::InvalidArgumentError(
        "Unsealing capability is not a valid capability");
  } else if (source.IsSealed()) {
    status =
        absl::InvalidArgumentError("Cannot unseal using a sealed capability");
  } else if ((source.permissions() & PermissionBits::kPermitUnseal) == 0) {
    status = absl::PermissionDeniedError("Missing unsealing permission");
  } else if (!source.IsValid()) {
    status =
        absl::InvalidArgumentError("Unsealing capability address out of range");
  } else if (obj_type != object_type()) {
    status =
        absl::InvalidArgumentError("Unsealing capability object type mismatch");
  }
  // Unseal the capability.
  set_object_type(kUnsealed);
  return status;
}

bool CheriotRegister::IsUnsealed() const {
  return tag() && (object_type() == kUnsealed);
}

bool CheriotRegister::IsRepresentable() const {
  if (exponent_ == 24) return true;
  uint64_t address = data_buffer()->Get<uint32_t>(0);
  uint64_t cap_base = base();
  return (cap_base <= address) &&
         (address < (cap_base + (1ULL << (exponent_ + 9))));
}

bool CheriotRegister::IsSentry() const {
  return !is_null_ && (object_type() >= kInterruptInheritingSentry) &&
         (object_type() <= kInterruptEnablingBackwardSentry);
}

bool CheriotRegister::IsBackwardSentry() const {
  return !is_null_ && ((object_type() == kInterruptEnablingBackwardSentry) ||
                       (object_type() == kInterruptDisablingBackwardSentry));
}

void CheriotRegister::CopyFrom(const CheriotRegister& other) {
  data_buffer()->CopyFrom(other.data_buffer());
  if (other.is_null_) {
    Expand(address(), 0, /*tag=*/false);
    return;
  }
  is_null_ = false;
  set_tag(other.tag());
  set_top(other.top());
  set_base(other.base());
  set_permissions(other.permissions());
  set_object_type(other.object_type());
  set_reserved(other.reserved());
  exponent_ = other.exponent();
  is_dirty_ = other.is_dirty_;
  raw_ = other.raw_;
}

bool CheriotRegister::operator==(const CheriotRegister& other) const {
  return (tag() == other.tag()) && (top() == other.top()) &&
         (base() == other.base()) && (permissions() == other.permissions()) &&
         (object_type() == other.object_type()) &&
         (reserved() == other.reserved());
}

bool CheriotRegister::IsMemoryEqual(const CheriotRegister& other) const {
  return (address() == other.address()) && (Compress() == other.Compress());
}

void CheriotRegister::SetAddress(uint32_t address) {
  if (!tag()) {
    data_buffer()->Set<uint32_t>(0, address);
    uint64_t mask = ~((1ULL << (exponent_ + 9)) - 1);
    auto len = length();
    set_base(address & mask);
    set_top(static_cast<uint64_t>(base()) + len);
    return;
  }
  set_address(address);
}

// Create a text representation of the capability.
std::string CheriotRegister::AsString() const {
  std::string output;
  absl::StrAppend(&output, absl::StrFormat("0x%08X", address()));
  absl::StrAppend(
      &output, " (v:", tag() ? "1 " : "0 ", absl::StrFormat("0x%08X", base()),
      "-", absl::StrFormat("0x%09X", top()),
      " l:", absl::StrFormat("0x%09X", length()), " o:",
      absl::StrFormat(
          "0x%X",
          object_type() |
              ((object_type() && !(permissions() & kPermitExecute)) ? 0x8
                                                                    : 0x0)),
      " p:", permissions() & kPermitGlobal ? "G " : "- ",
      permissions() & kPermitLoad ? "R" : "-",
      permissions() & kPermitStore ? "W" : "-",
      permissions() & kPermitLoadStoreCapability ? "c" : "-",
      permissions() & kPermitLoadMutable ? "m" : "-",
      permissions() & kPermitLoadGlobal ? "g" : "-",
      permissions() & kPermitStoreLocalCapability ? "l " : "- ",
      permissions() & kPermitExecute ? "X" : "-",
      permissions() & kPermitAccessSystemRegisters ? "a " : "- ",
      permissions() & kPermitSeal ? "S" : "-",
      permissions() & kPermitUnseal ? "U" : "-",
      permissions() & kUserPerm0 ? "0)" : "-)");
  return output;
}

// Determine the current permission format.
PermissionFormats CheriotRegister::GetPermissionFormat() const {
  if (is_null_) return kSealing;
  // Check to see if it is an executable permission format.
  if ((permissions() & kImpliedCapabilities[kExecutable]) ==
      kImpliedCapabilities[kExecutable]) {
    return kExecutable;
  }

  // Check to see if it is a mem_cap_rw format.
  if ((permissions() & kImpliedCapabilities[kMemoryCapReadWrite]) ==
      kImpliedCapabilities[kMemoryCapReadWrite]) {
    return kMemoryCapReadWrite;
  }

  // Check to see if it is a mem_cap_ro format.
  if ((permissions() & kImpliedCapabilities[kMemoryCapReadOnly]) ==
      kImpliedCapabilities[kMemoryCapReadOnly]) {
    return kMemoryCapReadOnly;
  }

  // Check to see if it is a mem_cap_wo format.
  if ((permissions() & kImpliedCapabilities[kMemoryCapWriteOnly]) ==
      kImpliedCapabilities[kMemoryCapWriteOnly]) {
    return kMemoryCapWriteOnly;
  }

  // Check to see if is a memory data only format.
  if ((permissions() & kPermitLoad) || (permissions() & kPermitStore)) {
    return kMemoryDataOnly;
  }

  return kSealing;
}

uint32_t CheriotRegister::CompressPermissions() const {
  // Determine the target format based on the currently set permissions.
  if (is_null_) return 0;

  auto format = GetPermissionFormat();
  uint32_t compressed;
  switch (format) {
    case kMemoryCapReadWrite:
      compressed = 0b0'11'000;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      compressed |= ((permissions() & kPermitStoreLocalCapability) != 0) << 2;
      compressed |= ((permissions() & kPermitLoadMutable) != 0) << 1;
      compressed |= ((permissions() & kPermitLoadGlobal) != 0);
      return compressed;
    case kMemoryCapReadOnly:
      compressed = 0b0'101'00;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      compressed |= ((permissions() & kPermitLoadMutable) != 0) << 1;
      compressed |= ((permissions() & kPermitLoadGlobal) != 0);
      return compressed;
    case kMemoryCapWriteOnly:
      compressed = 0b0'10000;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      return compressed;
    case kExecutable:
      compressed = 0b0'01'000;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      compressed |= ((permissions() & kPermitAccessSystemRegisters) != 0) << 2;
      compressed |= ((permissions() & kPermitLoadMutable) != 0) << 1;
      compressed |= ((permissions() & kPermitLoadGlobal) != 0);
      return compressed;
    case kMemoryDataOnly:
      compressed = 0b0'100'00;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      compressed |= ((permissions() & kPermitLoad) != 0) << 1;
      compressed |= ((permissions() & kPermitStore) != 0);
      return compressed;
    default:
      compressed = 0b0'00'000;
      compressed |= ((permissions() & kPermitGlobal) != 0) << 5;
      compressed |= ((permissions() & kUserPerm0) != 0) << 2;
      compressed |= ((permissions() & kPermitSeal) != 0) << 1;
      compressed |= ((permissions() & kPermitUnseal) != 0);
      return compressed;
  }
}

uint32_t CheriotRegister::ExpandPermissions(uint32_t compressed) const {
  // Determine the source compressed format based on table lookup.
  PermissionFormats format = kPermissionFormat[compressed & 0x1f];
  // First add the implied capabilities using table lookup.
  uint32_t expanded = kImpliedCapabilities[format];
  // Add the global format if present.
  expanded |= compressed & 0b1'00000 ? kPermitGlobal : 0;
  // Perform permission expansion based on format using table lookup.
  switch (format) {
    case kSealing:
      expanded |= kExpandSealed[compressed & 0b111];
      return expanded;
    case kExecutable:
      expanded |= kExpandExecutable[compressed & 0b111];
      return expanded;
    case kMemoryCapWriteOnly:
      // Only implied permissions, so just return the expanded value.
      return expanded;
    case kMemoryDataOnly:
      expanded |= kExpandMemoryDataOnly[compressed & 0b11];
      return expanded;
    case kMemoryCapReadOnly:
      expanded |= kExpandMemoryCapReadOnly[compressed & 0b11];
      return expanded;
    case kMemoryCapReadWrite:
      expanded |= kExpandMemoryCapReadWrite[compressed & 0b111];
      return expanded;
  }
}

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact
