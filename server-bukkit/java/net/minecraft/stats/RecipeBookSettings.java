// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.EnumMap;
import java.util.Map;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.inventory.RecipeBookType;
import org.basaltmc.bukkit.facade.LazyCodecs;

public final class RecipeBookSettings {
    public static final StreamCodec<FriendlyByteBuf, RecipeBookSettings> STREAM_CODEC = StreamCodec.of(
        (buf, settings) -> {
            for (RecipeBookType type : RecipeBookType.values()) {
                TypeSettings entry = settings.getSettings(type);
                buf.writeBoolean(entry.open());
                buf.writeBoolean(entry.filtering());
            }
        },
        buf -> {
            RecipeBookSettings settings = new RecipeBookSettings();
            for (RecipeBookType type : RecipeBookType.values()) {
                settings.states.put(type, new TypeSettings(buf.readBoolean(), buf.readBoolean()));
            }
            return settings;
        });
    public static final MapCodec<RecipeBookSettings> MAP_CODEC = LazyCodecs.mapCodec(RecipeBookSettings.class, "MAP_CODEC");

    private final Map<RecipeBookType, TypeSettings> states = new EnumMap<>(RecipeBookType.class);

    public RecipeBookSettings() {
        for (RecipeBookType type : RecipeBookType.values()) {
            states.put(type, TypeSettings.DEFAULT);
        }
    }

    private RecipeBookSettings(TypeSettings crafting, TypeSettings furnace, TypeSettings blastFurnace, TypeSettings smoker) {
        states.put(RecipeBookType.CRAFTING, crafting);
        states.put(RecipeBookType.FURNACE, furnace);
        states.put(RecipeBookType.BLAST_FURNACE, blastFurnace);
        states.put(RecipeBookType.SMOKER, smoker);
    }

    public TypeSettings getSettings(RecipeBookType type) {
        return states.get(type);
    }

    public boolean isOpen(RecipeBookType type) {
        return states.get(type).open();
    }

    public void setOpen(RecipeBookType type, boolean open) {
        states.compute(type, (k, v) -> v.setOpen(open));
    }

    public boolean isFiltering(RecipeBookType type) {
        return states.get(type).filtering();
    }

    public void setFiltering(RecipeBookType type, boolean filtering) {
        states.compute(type, (k, v) -> v.setFiltering(filtering));
    }

    public RecipeBookSettings copy() {
        return new RecipeBookSettings(states.get(RecipeBookType.CRAFTING), states.get(RecipeBookType.FURNACE),
            states.get(RecipeBookType.BLAST_FURNACE), states.get(RecipeBookType.SMOKER));
    }

    public void replaceFrom(RecipeBookSettings other) {
        states.clear();
        states.putAll(other.states);
    }

    public static record TypeSettings(boolean open, boolean filtering) {
        public static final TypeSettings DEFAULT = new TypeSettings(false, false);
        public static final MapCodec<TypeSettings> CRAFTING_MAP_CODEC = LazyCodecs.mapCodec(TypeSettings.class, "CRAFTING_MAP_CODEC");
        public static final MapCodec<TypeSettings> FURNACE_MAP_CODEC = LazyCodecs.mapCodec(TypeSettings.class, "FURNACE_MAP_CODEC");
        public static final MapCodec<TypeSettings> BLAST_FURNACE_MAP_CODEC = LazyCodecs.mapCodec(TypeSettings.class, "BLAST_FURNACE_MAP_CODEC");
        public static final MapCodec<TypeSettings> SMOKER_MAP_CODEC = LazyCodecs.mapCodec(TypeSettings.class, "SMOKER_MAP_CODEC");
        public static final StreamCodec<ByteBuf, TypeSettings> STREAM_CODEC = StreamCodec.of(
            (buf, value) -> {
                buf.writeBoolean(value.open());
                buf.writeBoolean(value.filtering());
            },
            buf -> new TypeSettings(buf.readBoolean(), buf.readBoolean()));

        public String toString() {
            return "[open=" + open + ", filtering=" + filtering + "]";
        }

        public TypeSettings setOpen(boolean open) {
            return new TypeSettings(open, filtering);
        }

        public TypeSettings setFiltering(boolean filtering) {
            return new TypeSettings(open, filtering);
        }
    }
}
