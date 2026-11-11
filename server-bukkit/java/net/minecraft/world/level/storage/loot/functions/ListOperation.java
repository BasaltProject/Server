// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.util.StringRepresentable;
public interface ListOperation {
    MapCodec<ListOperation> UNLIMITED_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ListOperation.class, "UNLIMITED_CODEC");
    static MapCodec<ListOperation> codec(int maxSize) {
        return (MapCodec<ListOperation>) UNLIMITED_CODEC;
    }
    Type mode();
    default <T> List<T> apply(List<T> original, List<T> replacement) {
        return apply(original, replacement, Integer.MAX_VALUE);
    }
    <T> List<T> apply(List<T> original, List<T> replacement, int maxSize);
    public static enum Type implements StringRepresentable {
        REPLACE_ALL, REPLACE_SECTION, INSERT, APPEND;
        public static final Codec<Type> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.ListOperation.Type.class, "CODEC");
        public MapCodec<? extends ListOperation> mapCodec() {
            return switch (this) {
                case REPLACE_ALL -> ReplaceAll.MAP_CODEC;
                case REPLACE_SECTION -> ReplaceSection.MAP_CODEC;
                case INSERT -> Insert.MAP_CODEC;
                case APPEND -> Append.MAP_CODEC;
            };
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static class ReplaceAll implements ListOperation {
        public static final ReplaceAll INSTANCE = new ReplaceAll();
        public static final MapCodec<ReplaceAll> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ListOperation.ReplaceAll.class, "MAP_CODEC");
        private ReplaceAll() {
            
        }
        public Type mode() {
            return Type.REPLACE_ALL;
        }
        public <T> List<T> apply(List<T> original, List<T> replacement, int maxSize) {
            return replacement;
        }
    }
    public static record ReplaceSection(int offset, Optional<Integer> size) implements ListOperation {
        public static final MapCodec<ReplaceSection> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ListOperation.ReplaceSection.class, "MAP_CODEC");
        public ReplaceSection(int offset) {
            this(offset, Optional.empty());
        }
        public Type mode() {
            return Type.REPLACE_SECTION;
        }
        public <T> List<T> apply(List<T> original, List<T> replacement, int maxSize) {
            int have = original.size();
            if (offset > have) {
                return original;
            }
            List<T> merged = new java.util.ArrayList<>(original.subList(0, offset));
            merged.addAll(replacement);
            int resume = offset + size.orElse(replacement.size());
            if (resume < have) {
                merged.addAll(original.subList(resume, have));
            }
            return merged.size() > maxSize ? original : List.copyOf(merged);
        }
    }
    public static record Insert(int offset) implements ListOperation {
        public static final MapCodec<Insert> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ListOperation.Insert.class, "MAP_CODEC");
        public Type mode() {
            return Type.INSERT;
        }
        public <T> List<T> apply(List<T> original, List<T> replacement, int maxSize) {
            int have = original.size();
            if (offset > have || have + replacement.size() > maxSize) {
                return original;
            }
            List<T> merged = new java.util.ArrayList<>(original.subList(0, offset));
            merged.addAll(replacement);
            merged.addAll(original.subList(offset, have));
            return List.copyOf(merged);
        }
    }
    public static class Append implements ListOperation {
        public static final Append INSTANCE = new Append();
        public static final MapCodec<Append> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ListOperation.Append.class, "MAP_CODEC");
        private Append() {
            
        }
        public Type mode() {
            return Type.APPEND;
        }
        public <T> List<T> apply(List<T> original, List<T> replacement, int maxSize) {
            if (original.size() + replacement.size() > maxSize) {
                return original;
            }
            List<T> merged = new java.util.ArrayList<>(original);
            merged.addAll(replacement);
            return List.copyOf(merged);
        }
    }
    public static record StandAlone<T>(List<T> value, ListOperation operation) {
        public static <T> Codec<StandAlone<T>> codec(Codec<T> valueCodec, int maxSize) {
            return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
                valueCodec.sizeLimitedListOf(maxSize).fieldOf("values").forGetter(StandAlone::value),
                ListOperation.codec(maxSize).forGetter(StandAlone::operation)).apply(i, StandAlone::new));
        }
        public List<T> apply(List<T> input) {
            return operation.apply(input, value);


        }
    }
}
