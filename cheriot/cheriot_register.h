/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MPACT_CHERIOT__CHERIOT_REGISTER_H_
#define MPACT_CHERIOT__CHERIOT_REGISTER_H_

// This file contains the definition of the Capability class used to implement
// the CHERI RiscV IoT capability.

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "mpact/sim/generic/register.h"

// This file defines the CHERI RiscV 64 bit capability register for use in
// a unified register file as described in CHERIoT.

namespace mpact {
namespace sim {
namespace cheriot {

class CheriotState;

class CheriotRegister : public generic::Register<uint32_t> {
 public:
  // Set of permission bits in uncompressed view.
  enum PermissionBits : uint32_t {
    kPermitNone = 0,
    kPermitGlobal = 1 << 0,
    kPermitLoadGlobal = 1 << 1,
    kPermitStore = 1 << 2,
    kPermitLoadMutable = 1 << 3,
    kPermitStoreLocalCapability = 1 << 4,
    kPermitLoad = 1 << 5,
    kPermitLoadStoreCapability = 1 << 6,
    kPermitAccessSystemRegisters = 1 << 7,
    kPermitExecute = 1 << 8,
    kPermitUnseal = 1 << 9,
    kPermitSeal = 1 << 10,
    kUserPerm0 = 1 << 11,
    kPermitMask = (1 << 12) - 1,
  };
  // In the compressed representation of the capability, the permission bits
  // are stored in one of the below compressed formats.
  enum PermissionFormats : uint32_t {
    kMemoryCapReadWrite,
    kMemoryCapReadOnly,
    kMemoryCapWriteOnly,
    kMemoryDataOnly,
    kExecutable,
    kSealing,
  };
  // Special object types.
  enum ObjectType : uint32_t {
    kUnsealed = 0,
    kInterruptInheritingSentry = 1,
    kInterruptDisablingForwardSentry = 2,
    kInterruptEnablingForwardSentry = 3,
    kInterruptDisablingBackwardSentry = 4,
    kInterruptEnablingBackwardSentry = 5,
    kSealedExecutable6 = 6,
    kSealedExecutable7 = 7,
    kReserved8 = 8,
    // 9-15 are sealed non-executable capabilities.
  };

  static constexpr uint32_t kNullCapability = 0;
  static constexpr unsigned kCapabilitySizeInBytes = 8;
  static constexpr unsigned kGranuleShift = 3;

  // Value type of this register class.
  using ValueType = uint32_t;

  // {msb, lsb} pairs of fields in the compressed capability.
  // Bit layout:
  // |3|3    2|2   2|2   1|1       |         |
  // |1|0    5|4   2|1   8|7      9|8       0|
  //  R  perm  otype  exp   base       top
  //
  static constexpr int kBase[2] = {8, 0};
  static constexpr int kTop[2] = {17, 9};
  static constexpr int kExponent[2] = {21, 18};
  static constexpr int kObjectType[2] = {24, 22};
  static constexpr int kPermissions[2] = {30, 25};
  static constexpr int kReserved[2] = {31, 31};

  // Constructor. Default constructors and assignment are disabled.
  CheriotRegister(generic::ArchState* state, absl::string_view name);
  CheriotRegister() = delete;
  CheriotRegister(const CheriotRegister&) = delete;
  CheriotRegister& operator=(const CheriotRegister&) = delete;

  // These functions set the capability register to a known state, either the
  // null capability, or one of the three root capabilities. All capabilities
  // have to be derived from a root capability.
  void ResetNull();
  void ResetMemoryRoot();
  void ResetExecuteRoot();
  void ResetSealingRoot();
  // Return true if the capability register has the given permission.
  bool HasPermission(uint32_t permission_bits) const {
    return (permissions() & permission_bits) != 0;
  }
  // Clears the capability as a null capability, but does not change the
  // address.
  void Clear();
  // Removes one or more permission from the current capability.
  void ClearPermissions(uint32_t permission_bits);
  // Clear the tag - invalidates the capability.
  void ClearTag();
  // Compute the bounds.
  std::pair<uint32_t, uint64_t> ComputeBounds();
  // Get the compressed representation of the capability.
  uint32_t Compress() const;
  // Expand the compressed capability representation.
  void Expand(uint32_t address, uint32_t compressed, bool tag);
  // If the address is out of range, invalidate the tag.
  void Validate();
  // Return true if the current capability is valid, i.e., tag is true and
  // the address is in range.
  bool IsValid() const;
  // Is representable.
  bool IsRepresentable() const;
  // Seal (unseal) the current capability based on permissions and address field
  // in source. Returns ok status if the operation is successful.
  absl::Status Seal(const CheriotRegister& source, uint32_t obj_type);
  absl::Status Unseal(const CheriotRegister& source, uint32_t obj_type);
  // Returns true if the capability is sealed, i.e., that the object type is set
  // to a valid, non-reserved object type.
  bool IsSealed() const;
  // Returns true if the capability is unsealed, i.e., that the object type is
  // zero.
  bool IsUnsealed() const;
  // Returns true if the capability is a sentry.
  bool IsSentry() const;
  // Returns true if the capability is a backward sentry.
  bool IsBackwardSentry() const;
  // Clears the tag.
  void Invalidate() { set_tag(false); }
  // Set bounds, return true if they're precise, i.e., that the base and length
  // do not have to be rounded, false otherwise.
  bool SetBounds(uint32_t req_base, uint64_t req_length);
  // Return true if the address is in bounds of the capability.
  bool IsInBounds(uint32_t cap_address, uint32_t size) const {
    return (cap_address >= base()) &&
           (top() >= (uint64_t)cap_address + (uint64_t)size);
  }
  // Copy fields from other capability register.
  void CopyFrom(const CheriotRegister& other);
  // Equal operator.
  bool operator==(const CheriotRegister& other) const;
  bool IsMemoryEqual(const CheriotRegister& other) const;
  // Text representation.
  std::string AsString() const;
  // Update address with change to base and top as needed.
  void SetAddress(uint32_t address);
  // Accessors.
  bool tag() const { return is_null_ ? false : tag_; }
  void set_tag(bool tag) { tag_ = tag; }

  uint32_t address() const { return data_buffer()->Get<uint32_t>(0); }
  void set_address(uint32_t address) {
    data_buffer()->Set<uint32_t>(0, address);
    Validate();
  }

  uint64_t top() const { return is_null_ ? address() & ~0x1ffULL : top_; }
  uint32_t base() const { return is_null_ ? address() & ~0x1ffULL : base_; }
  uint64_t length() const {
    // Length is only 33 bits, so mask off the value.
    return is_null_ ? 0 : (top_ - base_) & 0x1'ffff'ffffULL;
  }
  uint32_t exponent() const { return is_null_ ? 0 : exponent_; }
  uint32_t permissions() const { return is_null_ ? 0 : permissions_; }
  void set_permissions(uint32_t permissions) { permissions_ = permissions; }

  uint32_t object_type() const { return is_null_ ? 0 : object_type_; }
  void set_object_type(uint32_t object_type) {
    object_type_ = object_type & 0xf;
  }

  uint32_t reserved() const { return is_null_ ? 0 : reserved_; }
  void set_reserved(uint32_t reserved) { reserved_ = reserved & 0x1; }

  bool is_null() const { return is_null_; }
  void set_is_null() { is_null_ = true; }

 private:
  // These are the capabilities in each compressed capability permission format
  // that are writable.
  static constexpr uint32_t kWritableCapabilites[] = {
      /* kMemoryCapReadWrite */ kPermitGlobal | kPermitStoreLocalCapability |
          kPermitLoadMutable | kPermitLoadGlobal,
      /* kMemoryCapReadOnly */ kPermitGlobal | kPermitLoadMutable |
          kPermitLoadGlobal,
      /* kMemoryCapWriteOnly */ kPermitGlobal,
      /* kMemoryDataOnly */ kPermitGlobal | kPermitLoad | kPermitStore,
      /* kExecutable */ kPermitGlobal | kPermitAccessSystemRegisters |
          kPermitLoadMutable | kPermitLoadGlobal,
      /* kSealing */ kPermitGlobal | kUserPerm0 | kPermitSeal | kPermitUnseal,
  };

  // The different compressed capability permission formats have different sets
  // of implied capabilities.
  static constexpr uint32_t kImpliedCapabilities[] = {
      /* kMemoryCapReadWrite */ kPermitLoad | kPermitLoadStoreCapability |
          kPermitStore,
      /* kMemoryCapReadOnly */ kPermitLoad | kPermitLoadStoreCapability,
      /* kMemoryCapWriteOnly */ kPermitStore | kPermitLoadStoreCapability,
      /* kMemoryDataOnly */ 0,
      /* kExecutable */ kPermitExecute | kPermitLoad |
          kPermitLoadStoreCapability,
      /* kSealing */ 0,
  };

  // Decoding table for compressed permission formats.
  static constexpr PermissionFormats kPermissionFormat[] = {
      /* 00000 */ kSealing,
      /* 00001 */ kSealing,
      /* 00010 */ kSealing,
      /* 00011 */ kSealing,
      /* 00100 */ kSealing,
      /* 00101 */ kSealing,
      /* 00110 */ kSealing,
      /* 00111 */ kSealing,
      /* 01000 */ kExecutable,
      /* 01001 */ kExecutable,
      /* 01010 */ kExecutable,
      /* 01011 */ kExecutable,
      /* 01100 */ kExecutable,
      /* 01101 */ kExecutable,
      /* 01110 */ kExecutable,
      /* 01111 */ kExecutable,
      /* 10000 */ kMemoryCapWriteOnly,
      /* 10001 */ kMemoryDataOnly,
      /* 10010 */ kMemoryDataOnly,
      /* 10011 */ kMemoryDataOnly,
      /* 10100 */ kMemoryCapReadOnly,
      /* 10101 */ kMemoryCapReadOnly,
      /* 10110 */ kMemoryCapReadOnly,
      /* 10111 */ kMemoryCapReadOnly,
      /* 11000 */ kMemoryCapReadWrite,
      /* 11001 */ kMemoryCapReadWrite,
      /* 11010 */ kMemoryCapReadWrite,
      /* 11011 */ kMemoryCapReadWrite,
      /* 11100 */ kMemoryCapReadWrite,
      /* 11101 */ kMemoryCapReadWrite,
      /* 11110 */ kMemoryCapReadWrite,
      /* 11111 */ kMemoryCapReadWrite,
  };

  // Expansion table for permissions in the sealing format.
  static constexpr uint32_t kExpandSealed[8] = {
      kPermitNone,
      kPermitUnseal,
      kPermitSeal,
      kPermitUnseal | kPermitSeal,
      kUserPerm0,
      kUserPerm0 | kPermitUnseal,
      kUserPerm0 | kPermitSeal,
      kUserPerm0 | kPermitUnseal | kPermitSeal,
  };
  // Expansion table for permissions in the executable format.
  static constexpr uint32_t kExpandExecutable[8] = {
      kPermitNone,
      kPermitLoadGlobal,
      kPermitLoadMutable,
      kPermitLoadMutable | kPermitLoadGlobal,
      kPermitAccessSystemRegisters,
      kPermitAccessSystemRegisters | kPermitLoadGlobal,
      kPermitAccessSystemRegisters | kPermitLoadMutable,
      kPermitAccessSystemRegisters | kPermitLoadMutable | kPermitLoadGlobal,
  };
  // Expansion table for permissions in the memory data only format.
  static constexpr uint32_t kExpandMemoryDataOnly[4] = {
      kPermitNone,
      kPermitStore,
      kPermitLoad,
      kPermitStore | kPermitLoad,
  };
  // Expansion table for permissions in the memory cap read only format.
  static constexpr uint32_t kExpandMemoryCapReadOnly[4] = {
      kPermitNone,
      kPermitLoadGlobal,
      kPermitLoadMutable,
      kPermitLoadMutable | kPermitLoadGlobal,
  };
  // Expansion table for permissions in the memory cap read/write format.
  static constexpr uint32_t kExpandMemoryCapReadWrite[8] = {
      kPermitNone,
      kPermitLoadGlobal,
      kPermitLoadMutable,
      kPermitLoadMutable | kPermitLoadGlobal,
      kPermitStoreLocalCapability,
      kPermitStoreLocalCapability | kPermitLoadGlobal,
      kPermitStoreLocalCapability | kPermitLoadMutable,
      kPermitStoreLocalCapability | kPermitLoadMutable | kPermitLoadGlobal,
  };
  // Get the permission format based on the current permissions.
  PermissionFormats GetPermissionFormat() const;
  // Return the current permissions in compressed form.
  uint32_t CompressPermissions() const;
  // Return the expanded view of the given compressed form of permissions.
  uint32_t ExpandPermissions(uint32_t compressed) const;

  // If top or base is changed, set is_dirty_ so that the values get properly
  // compressed if written to memory.
  void set_top(uint64_t top) {
    top_ = top;
    is_dirty_ = true;
  }
  void set_base(uint32_t base) {
    base_ = base;
    is_dirty_ = true;
  }

  PermissionFormats permissions_format() const { return permissions_format_; }
  void set_permissions_format(PermissionFormats format) {
    permissions_format_ = format;
  }
  bool tag_ = false;
  uint64_t top_;
  uint32_t base_;
  // Stores the 12 permissions.
  uint32_t permissions_;
  PermissionFormats permissions_format_;
  // Stores the 3 bit object type.
  uint32_t object_type_;
  uint32_t reserved_;
  bool is_dirty_ = false;
  bool is_null_ = false;
  uint32_t raw_ = 0xdeadbeef;
  uint32_t exponent_ = 0;
};

}  // namespace cheriot
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_CHERIOT__CHERIOT_REGISTER_H_
