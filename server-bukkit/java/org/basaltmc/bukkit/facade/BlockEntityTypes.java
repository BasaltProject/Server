package org.basaltmc.bukkit.facade;

import java.util.HashMap;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.entity.BlockEntity;
import org.basaltmc.bukkit.WorldNatives;

public final class BlockEntityTypes {
    private static volatile Map<String, String> kTypes;

    private BlockEntityTypes() {}

    private static Map<String, String> types() {
        Map<String, String> found = kTypes;
        if (found == null) {
            String[] pairs = WorldNatives.blockEntityTypes();
            found = new HashMap<>();
            for (int i = 0; i + 1 < pairs.length; i += 2) {
                found.put(pairs[i], pairs[i + 1]);
            }
            kTypes = found;
        }
        return found;
    }

    public static String typeOf(Identifier block) {
        return types().get(block.toString());
    }

    public static BlockEntity detached(Identifier block, BlockPos pos) {
        String type = types().get(block.toString());
        if (type == null) {
            return null;
        }
        CompoundTag tag = new CompoundTag();
        tag.putString("id", type);
        tag.putInt("x", pos.getX());
        tag.putInt("y", pos.getY());
        tag.putInt("z", pos.getZ());
        return view(null, pos, tag);
    }

    public static BlockEntity view(net.minecraft.server.level.ServerLevel level, BlockPos pos, CompoundTag tag) {
        String id = tag.getStringOr("id", "");
        if (!id.startsWith("minecraft:")) {
            return new BlockEntity(level, pos, tag);
        }
        switch (id.substring(10)) {
            case "vault":
                return new net.minecraft.world.level.block.entity.vault.VaultBlockEntity(level, pos, tag);
            case "sign":
                return new net.minecraft.world.level.block.entity.SignBlockEntity(level, pos, tag);
            case "hanging_sign":
                return new net.minecraft.world.level.block.entity.HangingSignBlockEntity(level, pos, tag);
            case "chest":
                return new net.minecraft.world.level.block.entity.ChestBlockEntity(level, pos, tag);
            case "trapped_chest":
                return new net.minecraft.world.level.block.entity.TrappedChestBlockEntity(level, pos, tag);
            case "barrel":
                return new net.minecraft.world.level.block.entity.BarrelBlockEntity(level, pos, tag);
            case "shulker_box":
                return new net.minecraft.world.level.block.entity.ShulkerBoxBlockEntity(level, pos, tag);
            case "dispenser":
                return new net.minecraft.world.level.block.entity.DispenserBlockEntity(level, pos, tag);
            case "dropper":
                return new net.minecraft.world.level.block.entity.DropperBlockEntity(level, pos, tag);
            case "hopper":
                return new net.minecraft.world.level.block.entity.HopperBlockEntity(level, pos, tag);
            case "banner":
                return new net.minecraft.world.level.block.entity.BannerBlockEntity(level, pos, tag);
            case "enchanting_table":
                return new net.minecraft.world.level.block.entity.EnchantingTableBlockEntity(level, pos, tag);
            case "skull":
                return new net.minecraft.world.level.block.entity.SkullBlockEntity(level, pos, tag);
            case "command_block":
                return new net.minecraft.world.level.block.entity.CommandBlockEntity(level, pos, tag);
            case "conduit":
                return new net.minecraft.world.level.block.entity.ConduitBlockEntity(level, pos, tag);
            case "comparator":
                return new net.minecraft.world.level.block.entity.ComparatorBlockEntity(level, pos, tag);
            case "daylight_detector":
                return new net.minecraft.world.level.block.entity.DaylightDetectorBlockEntity(level, pos, tag);
            case "bell":
                return new net.minecraft.world.level.block.entity.BellBlockEntity(level, pos, tag);
            case "beehive":
                return new net.minecraft.world.level.block.entity.BeehiveBlockEntity(level, pos, tag);
            case "structure_block":
                return new net.minecraft.world.level.block.entity.StructureBlockEntity(level, pos, tag);
            case "jigsaw":
                return new net.minecraft.world.level.block.entity.JigsawBlockEntity(level, pos, tag);
            case "campfire":
                return new net.minecraft.world.level.block.entity.CampfireBlockEntity(level, pos, tag);
            case "end_portal":
                return new net.minecraft.world.level.block.entity.TheEndPortalBlockEntity(level, pos, tag);
            case "creaking_heart":
                return new net.minecraft.world.level.block.entity.CreakingHeartBlockEntity(level, pos, tag);
            case "brushable_block":
                return new net.minecraft.world.level.block.entity.BrushableBlockEntity(level, pos, tag);
            case "copper_golem_statue":
                return new net.minecraft.world.level.block.entity.CopperGolemStatueBlockEntity(level, pos, tag);
            case "sculk_catalyst":
                return new net.minecraft.world.level.block.entity.SculkCatalystBlockEntity(level, pos, tag);
            case "furnace":
                return new net.minecraft.world.level.block.entity.FurnaceBlockEntity(level, pos, tag);
            case "blast_furnace":
                return new net.minecraft.world.level.block.entity.BlastFurnaceBlockEntity(level, pos, tag);
            case "smoker":
                return new net.minecraft.world.level.block.entity.SmokerBlockEntity(level, pos, tag);
            case "brewing_stand":
                return new net.minecraft.world.level.block.entity.BrewingStandBlockEntity(level, pos, tag);
            case "crafter":
                return new net.minecraft.world.level.block.entity.CrafterBlockEntity(level, pos, tag);
            case "beacon":
                return new net.minecraft.world.level.block.entity.BeaconBlockEntity(level, pos, tag);
            case "lectern":
                return new net.minecraft.world.level.block.entity.LecternBlockEntity(level, pos, tag);
            case "jukebox":
                return new net.minecraft.world.level.block.entity.JukeboxBlockEntity(level, pos, tag);
            case "mob_spawner":
                return new net.minecraft.world.level.block.entity.SpawnerBlockEntity(level, pos, tag);
            case "trial_spawner":
                return new net.minecraft.world.level.block.entity.TrialSpawnerBlockEntity(level, pos, tag);
            case "sculk_sensor":
                return new net.minecraft.world.level.block.entity.SculkSensorBlockEntity(level, pos, tag);
            case "sculk_shrieker":
                return new net.minecraft.world.level.block.entity.SculkShriekerBlockEntity(level, pos, tag);
            case "decorated_pot":
                return new net.minecraft.world.level.block.entity.DecoratedPotBlockEntity(level, pos, tag);
            case "ender_chest":
                return new net.minecraft.world.level.block.entity.EnderChestBlockEntity(level, pos, tag);
            default:
                return new BlockEntity(level, pos, tag);
        }
    }
}
