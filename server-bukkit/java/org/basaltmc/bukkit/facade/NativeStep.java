package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.Typed;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Dynamic;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import org.basaltmc.bukkit.LeftoverNatives;

/** Runs one vanilla data fix step in the native fixer, on a whole document or on a part of one. */
public final class NativeStep {
    private static final int FLAT_CHUNK_KEY = 28320;

    private NativeStep() {}

    /** The document of a data type after the step; throws when the native fixer cannot run it. */
    public static CompoundTag root(String type, String step, int versionKey, CompoundTag document) {
        byte[] fixed = LeftoverNatives.datafixStep(type, step, versionKey, NbtCodec.toAnonymous(document));
        if (fixed == null) {
            throw new IllegalStateException("The native data fixer cannot run " + step + " at " + versionKey + " on " + type);
        }
        return NbtCodec.fromAnonymous(fixed);
    }

    private static CompoundTag held(String key, CompoundTag part) {
        CompoundTag root = new CompoundTag();
        ListTag list = new ListTag();
        list.add(part);
        root.put(key, list);
        return root;
    }

    private static CompoundTag first(CompoundTag root, String key) {
        ListTag list = root.getListOrEmpty(key);
        return list.isEmpty() ? new CompoundTag() : list.getCompoundOrEmpty(0);
    }

    /** One entity, block entity or item stack (by kind) or a whole document of the named type, after the step. */
    public static CompoundTag part(String kind, String step, int versionKey, CompoundTag part) {
        switch (kind) {
            case "entity":
                return first(root("entity_chunk", step, versionKey, held("Entities", part)), "Entities");
            case "item_stack":
                return first(root("entity_chunk", step, versionKey, held("Items", part)), "Items");
            case "block_entity":
                if (versionKey >= FLAT_CHUNK_KEY) {
                    return first(root("chunk", step, versionKey, held("block_entities", part)), "block_entities");
                }
                CompoundTag level = held("TileEntities", part);
                CompoundTag chunk = new CompoundTag();
                chunk.put("Level", level);
                return first(root("chunk", step, versionKey, chunk).getCompoundOrEmpty("Level"), "TileEntities");
            default:
                return root(kind, step, versionKey, part);
        }
    }

    @SuppressWarnings("unchecked")
    public static <T> Dynamic<T> dynamic(String kind, String step, int versionKey, Dynamic<T> input) {
        Tag tag = input.convert(NbtOps.INSTANCE).getValue();
        CompoundTag fixed = part(kind, step, versionKey, (CompoundTag) tag);
        return new Dynamic<Tag>(NbtOps.INSTANCE, fixed).convert(input.getOps());
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Typed<?> typed(String kind, String step, int versionKey, Typed<?> input) {
        Dynamic<?> written = input.write().getOrThrow();
        Dynamic<?> fixed = dynamic(kind, step, versionKey, written);
        Pair<? extends Typed<?>, ?> read = (Pair<? extends Typed<?>, ?>) ((com.mojang.datafixers.types.Type) input.getType()).readTyped(fixed).getOrThrow();
        return read.getFirst();
    }

    /** An entity of a legacy fix named by its choice, after the step, with the id the step left it. */
    public static Pair<String, Dynamic<?>> entity(String name, Dynamic<?> tag, String step, int versionKey) {
        Dynamic<?> fixed = dynamic("entity", step, versionKey, tag.set("id", tag.createString(name)));
        return Pair.of(fixed.get("id").asString(name), fixed.remove("id"));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Pair<String, Typed<?>> entityTyped(String name, Typed<?> input, String step, int versionKey) {
        Dynamic<?> written = input.write().getOrThrow();
        Pair<String, Dynamic<?>> fixed = entity(name, written, step, versionKey);
        Pair<? extends Typed<?>, ?> read = (Pair<? extends Typed<?>, ?>) ((com.mojang.datafixers.types.Type) input.getType()).readTyped(fixed.getSecond()).getOrThrow();
        return Pair.of(fixed.getFirst(), read.getFirst());
    }
}
