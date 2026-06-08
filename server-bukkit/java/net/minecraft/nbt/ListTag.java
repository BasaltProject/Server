package net.minecraft.nbt;

import java.util.Iterator;
import java.util.Optional;
import org.basaltmc.bukkit.Unsupported;
public class ListTag implements CollectionTag {
    public ListTag() {
    }
    public boolean add(Object objectValue) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.add");
    }
    public void addAndUnwrap(Tag tag) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.addAndUnwrap");
    }
    public Tag get(int index) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.get");
    }
    public CompoundTag getCompoundOrEmpty(int index) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.getCompoundOrEmpty");
    }
    public float getFloatOr(int index, float defaultValue) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.getFloatOr");
    }
    public Optional getString(int index) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.getString");
    }
    public boolean isEmpty() {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.isEmpty");
    }
    public Iterator iterator() {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.iterator");
    }
    public Tag set(int index, Tag tag) {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.set");
    }
    public int size() {
        throw Unsupported.hit("net.minecraft.nbt.ListTag.size");
    }
}
