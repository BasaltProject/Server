package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.entity.AttributeOperation
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.nbt.deepCopy

/** Small NBT updater for concrete finalizers that add attributes after `Mob.finalizeSpawn`. */
internal object SpawnAttributeNbt {
    fun setBase(
        data: NbtCompound,
        attributeId: String,
        base: Double,
    ) {
        update(data, attributeId, base, null)
    }

    fun addModifier(
        data: NbtCompound,
        attributeId: String,
        base: Double,
        modifierId: String,
        amount: Double,
        operation: AttributeOperation,
    ) {
        val modifier =
            NbtCompound().apply {
                putString("id", modifierId)
                this["amount"] = NbtDouble(amount)
                putString("operation", operation.serializedName)
            }
        update(data, attributeId, base, modifier)
    }

    private fun update(
        data: NbtCompound,
        attributeId: String,
        base: Double,
        modifier: NbtCompound?,
    ) {
        val packed =
            (data["attributes"] as? NbtList)
                ?.filterIsInstance<NbtCompound>()
                ?.map { it.deepCopy() }
                ?.toMutableList() ?: mutableListOf()
        val existing = packed.firstOrNull { it.getString("id") == attributeId }
        val attribute =
            existing ?: NbtCompound().apply {
                putString("id", attributeId)
                this["base"] = NbtDouble(base)
                packed.add(this)
            }
        attribute["base"] = NbtDouble(base)
        if (modifier != null) {
            val modifiers =
                (attribute["modifiers"] as? NbtList)
                    ?.filterIsInstance<NbtCompound>()
                    ?.map { it.deepCopy() }
                    ?.toMutableList() ?: mutableListOf()
            modifiers.removeAll { it.getString("id") == modifier.getString("id") }
            modifiers.add(modifier)
            attribute["modifiers"] = NbtList(NbtType.COMPOUND, modifiers)
        }
        data["attributes"] = NbtList(NbtType.COMPOUND, packed)
    }
}
