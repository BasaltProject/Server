// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.material.rule;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import net.minecraft.world.level.levelgen.material.MaterialRules;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.densityfunction.DensityFunction;
import net.minecraft.world.level.levelgen.material.MaterialRuleContext;
public record OreVeinRule(BlockState oreBlock, BlockState rawOreBlock, BlockState fillerBlock, float rawOreChance, DensityFunction density, DensityFunction richness, DensityFunction fillerGap) implements MaterialRule {
    public static final float VEININESS_THRESHOLD = 0.4F;
    public static final int EDGE_ROUNDOFF_BEGIN = 20;
    public static final float MAX_EDGE_ROUNDOFF = 0.2F;
    public static final float VEIN_SOLIDNESS = 0.7F;
    public static final float MIN_RICHNESS = 0.1F;
    public static final float MAX_RICHNESS = 0.3F;
    public static final float MAX_RICHNESS_THRESHOLD = 0.6F;
    public static final float SKIP_ORE_IF_GAP_NOISE_IS_BELOW = -0.3F;
    public static final MapCodec<OreVeinRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.material.rule.OreVeinRule.class, "CODEC");
    public RuleEvaluator compile(MaterialRuleContext context) {
        MaterialRules.DensityGetter veinDensity = context.getDensitiesInChunk(density, true);
        MaterialRules.DensityGetter veinRichness = context.getDensitiesInChunk(richness, true);
        MaterialRules.DensityGetter gap = context.getDensitiesInChunk(fillerGap, false);
        PositionalRandomFactory factory = context.getOrCreateRandomFactory(Identifier.withDefaultNamespace("ore"));
        return (x, y, z) -> {
            float strength = veinDensity.get();
            if (strength <= 0.0F) {
                return null;
            }
            RandomSource random = factory.at(x, y, z);
            if (random.nextFloat() > strength) {
                return null;
            }
            if (random.nextFloat() < veinRichness.get() && gap.get() < 0.0F) {
                return random.nextFloat() < rawOreChance ? rawOreBlock : oreBlock;
            }
            return fillerBlock;
        };
    }
    public MapCodec<OreVeinRule> codec() {
        return CODEC;
    }
    public static enum VeinType {
        COPPER(Blocks.COPPER_ORE.defaultBlockState(), Blocks.RAW_COPPER_BLOCK.defaultBlockState(),
            Blocks.GRANITE.defaultBlockState(), 0, 50),
        IRON(Blocks.DEEPSLATE_IRON_ORE.defaultBlockState(), Blocks.RAW_IRON_BLOCK.defaultBlockState(),
            Blocks.TUFF.defaultBlockState(), -60, -8);

        private final BlockState ore;
        private final BlockState rawOre;
        private final BlockState filler;
        public final int minY;
        public final int maxY;

        private VeinType(BlockState ore, BlockState rawOre, BlockState filler, int minY, int maxY) {
            this.ore = ore;
            this.rawOre = rawOre;
            this.filler = filler;
            this.minY = minY;
            this.maxY = maxY;
        }

        public OreVeinRule create(DensityFunction density, DensityFunction richness, DensityFunction fillerGap) {
        return new OreVeinRule(ore, rawOre, filler, 0.02F, density, richness, fillerGap);

        }
    }
}
