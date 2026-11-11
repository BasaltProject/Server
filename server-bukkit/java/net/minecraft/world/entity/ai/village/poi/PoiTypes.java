// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.village.poi;
import com.google.common.collect.ImmutableSet;
import java.util.HashMap;
import java.util.Locale;
import java.util.Set;
import java.util.stream.Collectors;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.BedBlock;
import net.minecraft.world.level.block.state.properties.BedPart;
import java.util.Map;
import java.util.Set;
import java.util.HashMap;
import net.minecraft.world.level.block.Block;
import net.minecraft.resources.Identifier;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.state.BlockState;
public class PoiTypes {
    private static final Map<BlockState, Holder<PoiType>> kByState = new HashMap<>();
    private static Set<BlockState> statesOf(Block... blocks) {
        return java.util.Arrays.stream(blocks).flatMap(block -> block.getStateDefinition().getPossibleStates().stream()).collect(ImmutableSet.toImmutableSet());
    }
    private static PoiType put(Registry<PoiType> registry, ResourceKey<PoiType> id, Set<BlockState> states, int maxTickets, int validRange) {
        PoiType value = new PoiType(states, maxTickets, validRange);
        Registry.register(registry, id, value);
        Holder<PoiType> holder = registry.getOrThrow(id);
        for (BlockState state : states) {
            if (kByState.put(state, holder) != null) {
                throw new IllegalStateException(String.format(Locale.ROOT, "%s is defined in more than one PoI type", state));
            }
        }
        return value;
    }
    public static final ResourceKey<PoiType> ARMORER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("armorer"));
    public static final ResourceKey<PoiType> BUTCHER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("butcher"));
    public static final ResourceKey<PoiType> CARTOGRAPHER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("cartographer"));
    public static final ResourceKey<PoiType> CLERIC = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("cleric"));
    public static final ResourceKey<PoiType> FARMER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("farmer"));
    public static final ResourceKey<PoiType> FISHERMAN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("fisherman"));
    public static final ResourceKey<PoiType> FLETCHER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("fletcher"));
    public static final ResourceKey<PoiType> LEATHERWORKER = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("leatherworker"));
    public static final ResourceKey<PoiType> LIBRARIAN = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("librarian"));
    public static final ResourceKey<PoiType> MASON = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("mason"));
    public static final ResourceKey<PoiType> SHEPHERD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("shepherd"));
    public static final ResourceKey<PoiType> TOOLSMITH = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("toolsmith"));
    public static final ResourceKey<PoiType> WEAPONSMITH = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("weaponsmith"));
    public static final ResourceKey<PoiType> HOME = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("home"));
    public static final ResourceKey<PoiType> MEETING = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("meeting"));
    public static final ResourceKey<PoiType> BEEHIVE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("beehive"));
    public static final ResourceKey<PoiType> BEE_NEST = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("bee_nest"));
    public static final ResourceKey<PoiType> NETHER_PORTAL = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("nether_portal"));
    public static final ResourceKey<PoiType> LODESTONE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("lodestone"));
    public static final ResourceKey<PoiType> LIGHTNING_ROD = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("lightning_rod"));
    public static final ResourceKey<PoiType> TEST_INSTANCE = net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.POINT_OF_INTEREST_TYPE, net.minecraft.resources.Identifier.withDefaultNamespace("test_instance"));
    public static Optional<Holder<PoiType>> forState(BlockState state) {
        return Optional.ofNullable(kByState.get(state));
    }
    public static boolean hasPoi(BlockState state) {
        return kByState.containsKey(state);
    }
    public static PoiType bootstrap(Registry<PoiType> registry) {
        Set<BlockState> beds = Blocks.BED.asList().stream().flatMap(block -> block.getStateDefinition().getPossibleStates().stream())
            .filter(state -> state.getValue(BedBlock.PART) == BedPart.HEAD).collect(ImmutableSet.toImmutableSet());
        put(registry, ARMORER, statesOf(Blocks.BLAST_FURNACE), 1, 1);
        put(registry, BUTCHER, statesOf(Blocks.SMOKER), 1, 1);
        put(registry, CARTOGRAPHER, statesOf(Blocks.CARTOGRAPHY_TABLE), 1, 1);
        put(registry, CLERIC, statesOf(Blocks.BREWING_STAND), 1, 1);
        put(registry, FARMER, statesOf(Blocks.COMPOSTER), 1, 1);
        put(registry, FISHERMAN, statesOf(Blocks.BARREL), 1, 1);
        put(registry, FLETCHER, statesOf(Blocks.FLETCHING_TABLE), 1, 1);
        put(registry, LEATHERWORKER, statesOf(Blocks.CAULDRON, Blocks.LAVA_CAULDRON, Blocks.WATER_CAULDRON, Blocks.POWDER_SNOW_CAULDRON), 1, 1);
        put(registry, LIBRARIAN, statesOf(Blocks.LECTERN), 1, 1);
        put(registry, MASON, statesOf(Blocks.STONECUTTER), 1, 1);
        put(registry, SHEPHERD, statesOf(Blocks.LOOM), 1, 1);
        put(registry, TOOLSMITH, statesOf(Blocks.SMITHING_TABLE), 1, 1);
        put(registry, WEAPONSMITH, statesOf(Blocks.GRINDSTONE), 1, 1);
        put(registry, HOME, beds, 1, 1);
        put(registry, MEETING, statesOf(Blocks.BELL), 32, 6);
        put(registry, BEEHIVE, statesOf(Blocks.BEEHIVE), 0, 1);
        put(registry, BEE_NEST, statesOf(Blocks.BEE_NEST), 0, 1);
        put(registry, NETHER_PORTAL, statesOf(Blocks.NETHER_PORTAL), 0, 1);
        put(registry, LODESTONE, statesOf(Blocks.LODESTONE), 0, 1);
        put(registry, TEST_INSTANCE, statesOf(Blocks.TEST_INSTANCE_BLOCK), 0, 1);
        return put(registry, LIGHTNING_ROD, statesOf(Blocks.LIGHTNING_ROD.asList().toArray(new Block[0])), 0, 1);

    }
}
