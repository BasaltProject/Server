package net.minecraft.nbt;

import java.util.Optional;
import java.util.Set;
import org.basaltmc.bukkit.Unsupported;
public class CompoundTag implements Tag {
    public CompoundTag() {
    }
    public boolean contains(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.contains");
    }
    public Tag get(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.get");
    }
    public Optional getBoolean(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getBoolean");
    }
    public Optional getByte(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getByte");
    }
    public Optional getByteArray(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getByteArray");
    }
    public byte getByteOr(String name, byte defaultValue) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getByteOr");
    }
    public Optional getCompound(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getCompound");
    }
    public CompoundTag getCompoundOrEmpty(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getCompoundOrEmpty");
    }
    public Optional getFloat(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getFloat");
    }
    public Optional getInt(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getInt");
    }
    public int getIntOr(String name, int defaultValue) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getIntOr");
    }
    public Optional getList(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getList");
    }
    public ListTag getListOrEmpty(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getListOrEmpty");
    }
    public Optional getShort(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getShort");
    }
    public short getShortOr(String name, short defaultValue) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getShortOr");
    }
    public Optional getString(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.getString");
    }
    public boolean isEmpty() {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.isEmpty");
    }
    public Set keySet() {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.keySet");
    }
    public Tag put(String name, Tag tag) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.put");
    }
    public void putByte(String name, byte value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putByte");
    }
    public void putFloat(String name, float value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putFloat");
    }
    public void putInt(String name, int value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putInt");
    }
    public void putIntArray(String name, int[] value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putIntArray");
    }
    public void putShort(String name, short value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putShort");
    }
    public void putString(String name, String value) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.putString");
    }
    public Tag remove(String name) {
        throw Unsupported.hit("net.minecraft.nbt.CompoundTag.remove");
    }
}
