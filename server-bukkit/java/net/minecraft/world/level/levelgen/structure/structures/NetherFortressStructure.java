// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.util.random.WeightedList;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.level.biome.MobSpawnSettings;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureType;
public class NetherFortressStructure extends Structure {
    public static final WeightedList<MobSpawnSettings.SpawnerData> FORTRESS_ENEMIES = WeightedList.<MobSpawnSettings.SpawnerData>builder()
        .add(new MobSpawnSettings.SpawnerData(EntityTypes.BLAZE, new UniformInt(2, 3)), 10)
        .add(new MobSpawnSettings.SpawnerData(EntityTypes.ZOMBIFIED_PIGLIN, new ConstantInt(4)), 5)
        .add(new MobSpawnSettings.SpawnerData(EntityTypes.WITHER_SKELETON, new ConstantInt(5)), 8)
        .add(new MobSpawnSettings.SpawnerData(EntityTypes.SKELETON, new ConstantInt(5)), 2)
        .add(new MobSpawnSettings.SpawnerData(EntityTypes.MAGMA_CUBE, new ConstantInt(4)), 3).build();
    public static final MapCodec<NetherFortressStructure> CODEC = simpleCodec(NetherFortressStructure::new);
    public NetherFortressStructure(Structure.StructureSettings settings) {
        super(settings);
    }
    public Optional<Structure.GenerationStub> findGenerationPoint(Structure.GenerationContext context) {
        return Optional.empty();
    }
    public StructureType<?> type() {
        return StructureType.FORTRESS;
    }
}
