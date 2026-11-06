// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;

import com.mojang.serialization.Codec;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.stream.Stream;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.level.block.SoundType;

public record BlockSetType(String name, boolean canOpenByHand, boolean canOpenByWindCharge, boolean canButtonBeActivatedByArrows, PressurePlateSensitivity pressurePlateSensitivity, SoundType soundType, SoundEvent doorClose, SoundEvent doorOpen, SoundEvent trapdoorClose, SoundEvent trapdoorOpen, SoundEvent pressurePlateClickOff, SoundEvent pressurePlateClickOn, SoundEvent buttonClickOff, SoundEvent buttonClickOn) {
    private static final Map<String, BlockSetType> kTypes = new LinkedHashMap<>();
    public static final Codec<BlockSetType> CODEC = Codec.stringResolver(BlockSetType::name, kTypes::get);
    public static final BlockSetType IRON = register(family("iron", false, false, false, PressurePlateSensitivity.EVERYTHING, SoundType.IRON,
        "iron_door", "iron_trapdoor", "metal_pressure_plate", "stone_button"));
    public static final BlockSetType COPPER = register(family("copper", true, true, false, PressurePlateSensitivity.EVERYTHING, SoundType.COPPER,
        "copper_door", "copper_trapdoor", "metal_pressure_plate", "stone_button"));
    public static final BlockSetType GOLD = register(family("gold", false, true, false, PressurePlateSensitivity.EVERYTHING, SoundType.METAL,
        "iron_door", "iron_trapdoor", "metal_pressure_plate", "stone_button"));
    public static final BlockSetType STONE = register(family("stone", true, true, false, PressurePlateSensitivity.MOBS, SoundType.STONE,
        "iron_door", "iron_trapdoor", "stone_pressure_plate", "stone_button"));
    public static final BlockSetType POLISHED_BLACKSTONE = register(family("polished_blackstone", true, true, false, PressurePlateSensitivity.MOBS, SoundType.STONE,
        "iron_door", "iron_trapdoor", "stone_pressure_plate", "stone_button"));
    public static final BlockSetType OAK = register(new BlockSetType("oak"));
    public static final BlockSetType SPRUCE = register(new BlockSetType("spruce"));
    public static final BlockSetType BIRCH = register(new BlockSetType("birch"));
    public static final BlockSetType ACACIA = register(new BlockSetType("acacia"));
    public static final BlockSetType CHERRY = register(family("cherry", true, true, true, PressurePlateSensitivity.EVERYTHING, SoundType.CHERRY_WOOD,
        "cherry_wood_door", "cherry_wood_trapdoor", "cherry_wood_pressure_plate", "cherry_wood_button"));
    public static final BlockSetType JUNGLE = register(new BlockSetType("jungle"));
    public static final BlockSetType DARK_OAK = register(new BlockSetType("dark_oak"));
    public static final BlockSetType PALE_OAK = register(new BlockSetType("pale_oak"));
    public static final BlockSetType POPLAR = register(new BlockSetType("poplar"));
    public static final BlockSetType CRIMSON = register(family("crimson", true, true, true, PressurePlateSensitivity.EVERYTHING, SoundType.NETHER_WOOD,
        "nether_wood_door", "nether_wood_trapdoor", "nether_wood_pressure_plate", "nether_wood_button"));
    public static final BlockSetType WARPED = register(family("warped", true, true, true, PressurePlateSensitivity.EVERYTHING, SoundType.NETHER_WOOD,
        "nether_wood_door", "nether_wood_trapdoor", "nether_wood_pressure_plate", "nether_wood_button"));
    public static final BlockSetType MANGROVE = register(new BlockSetType("mangrove"));
    public static final BlockSetType BAMBOO = register(family("bamboo", true, true, true, PressurePlateSensitivity.EVERYTHING, SoundType.BAMBOO_WOOD,
        "bamboo_wood_door", "bamboo_wood_trapdoor", "bamboo_wood_pressure_plate", "bamboo_wood_button"));

    public BlockSetType(String name) {
        this(name, true, true, true, PressurePlateSensitivity.EVERYTHING, SoundType.WOOD,
            sound("wooden_door", "close"), sound("wooden_door", "open"), sound("wooden_trapdoor", "close"), sound("wooden_trapdoor", "open"),
            sound("wooden_pressure_plate", "click_off"), sound("wooden_pressure_plate", "click_on"),
            sound("wooden_button", "click_off"), sound("wooden_button", "click_on"));
    }

    private static SoundEvent sound(String block, String action) {
        return SoundEvents.byId("block." + block + "." + action);
    }

    private static BlockSetType family(String name, boolean byHand, boolean byWind, boolean byArrows, PressurePlateSensitivity sensitivity,
            SoundType soundType, String door, String trapdoor, String plate, String button) {
        return new BlockSetType(name, byHand, byWind, byArrows, sensitivity, soundType, sound(door, "close"), sound(door, "open"),
            sound(trapdoor, "close"), sound(trapdoor, "open"), sound(plate, "click_off"), sound(plate, "click_on"),
            sound(button, "click_off"), sound(button, "click_on"));
    }

    private static BlockSetType register(BlockSetType type) {
        kTypes.put(type.name, type);
        return type;
    }

    public static Stream<BlockSetType> values() {
        return kTypes.values().stream();
    }

    public static enum PressurePlateSensitivity {
        EVERYTHING, MOBS;
    }
}
