// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class MineshaftStructure extends Structure {
    private final Type type;
    public static final MapCodec<MineshaftStructure> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i),
        Type.CODEC.fieldOf("mineshaft_type").forGetter(c -> c.type)).apply(i, MineshaftStructure::new));
    public MineshaftStructure(Structure.StructureSettings settings, Type type) {
        super(settings);
        this.type = type;
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return Optional.empty();
    }
    public StructureType<?> type() {
        return StructureType.MINESHAFT;
    }
    public static enum Type implements StringRepresentable {
        NORMAL, MESA;
        public static final Codec<Type> CODEC = StringRepresentable.fromEnum(Type::values);
        public String getName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public static Type byId(int id) {
            Type[] all = values();
            return id >= 0 && id < all.length ? all[id] : NORMAL;
        }
        public BlockState getWoodState() {
            return (this == MESA ? Blocks.DARK_OAK_LOG : Blocks.OAK_LOG).defaultBlockState();
        }
        public BlockState getPlanksState() {
            return (this == MESA ? Blocks.DARK_OAK_PLANKS : Blocks.OAK_PLANKS).defaultBlockState();
        }
        public BlockState getFenceState() {
            return (this == MESA ? Blocks.DARK_OAK_FENCE : Blocks.OAK_FENCE).defaultBlockState();


        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
