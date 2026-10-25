package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.List;
import org.bukkit.ExplosionResult;
import org.bukkit.Material;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.block.BlockState;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;
import org.bukkit.event.block.BlockIgniteEvent;
import org.bukkit.inventory.ItemStack;
import org.bukkit.util.Vector;

/** The block events Basalt raises, as the Bukkit and Paper events plugins listen for. */
final class BlockEvents {
    private BlockEvents() {}

    private static BlockState becoming(Block block, String blockId) {
        BlockState state = block.getState();
        Material to = Material.matchMaterial(blockId);
        if (to != null) {
            state.setType(to);
        }
        return state;
    }

    private static boolean spreads(Material from, Material to) {
        return (from == Material.DIRT || from == Material.COARSE_DIRT || from == Material.ROOTED_DIRT
            || from == Material.PODZOL) && (to == Material.GRASS_BLOCK || to == Material.MYCELIUM)
            || from == Material.FIRE && to == Material.FIRE || to == Material.VINE;
    }

    private static boolean forms(Material from, Material to) {
        return to == Material.ICE || to == Material.SNOW || to == Material.SNOW_BLOCK || to == Material.FROSTED_ICE
            || to == Material.OBSIDIAN || to == Material.COBBLESTONE || to == Material.STONE
            || from == Material.WATER && to == Material.ICE;
    }

    private static BlockIgniteEvent.IgniteCause igniteCause(String name) {
        return switch (name) {
            case "spread" -> BlockIgniteEvent.IgniteCause.SPREAD;
            case "lava" -> BlockIgniteEvent.IgniteCause.LAVA;
            case "lightning" -> BlockIgniteEvent.IgniteCause.LIGHTNING;
            case "flint_and_steel", "flint" -> BlockIgniteEvent.IgniteCause.FLINT_AND_STEEL;
            case "fireball", "fire_charge" -> BlockIgniteEvent.IgniteCause.FIREBALL;
            case "explosion" -> BlockIgniteEvent.IgniteCause.EXPLOSION;
            case "arrow" -> BlockIgniteEvent.IgniteCause.ARROW;
            case "ender_crystal" -> BlockIgniteEvent.IgniteCause.ENDER_CRYSTAL;
            default -> BlockIgniteEvent.IgniteCause.SPREAD;
        };
    }

    static void install() {
        EventBridge.map("block.ignite", BlockIgniteEvent.class, f -> {
            Block block = f.block();
            Player who = f.text("player_name").isEmpty() ? null : BasaltServer.instance().playerByName(
                f.text("player_name"));
            return block == null ? null : new BlockIgniteEvent(block, igniteCause(f.text("cause")), (Entity) who);
        });
        EventBridge.map("block.fade", org.bukkit.event.block.BlockFadeEvent.class, f -> {
            Block block = f.block();
            Material from = Material.matchMaterial(f.text("block"));
            return block == null || (from != null && org.bukkit.Tag.LEAVES.isTagged(from)) ? null
                : new org.bukkit.event.block.BlockFadeEvent(block, becoming(block, f.text("to_block")));
        });
        EventBridge.map("block.grow", org.bukkit.event.block.BlockGrowEvent.class, f -> {
            Block block = f.block();
            Material from = Material.matchMaterial(f.text("block"));
            Material to = Material.matchMaterial(f.text("to_block"));
            if (block == null || from == null || to == null || spreads(from, to) || forms(from, to)) {
                return null;
            }
            return new org.bukkit.event.block.BlockGrowEvent(block, becoming(block, f.text("to_block")));
        });
        EventBridge.map("block.grow", org.bukkit.event.block.BlockSpreadEvent.class, f -> {
            Block block = f.block();
            Material from = Material.matchMaterial(f.text("block"));
            Material to = Material.matchMaterial(f.text("to_block"));
            if (block == null || from == null || to == null || !spreads(from, to)) {
                return null;
            }
            return new org.bukkit.event.block.BlockSpreadEvent(block, block.getRelative(BlockFace.UP),
                becoming(block, f.text("to_block")));
        });
        EventBridge.map("block.grow", org.bukkit.event.block.BlockFormEvent.class, f -> {
            Block block = f.block();
            Material from = Material.matchMaterial(f.text("block"));
            Material to = Material.matchMaterial(f.text("to_block"));
            if (block == null || from == null || to == null || !forms(from, to) || spreads(from, to)) {
                return null;
            }
            return new org.bukkit.event.block.BlockFormEvent(block, becoming(block, f.text("to_block")));
        });

        EventBridge.map("block.burn", org.bukkit.event.block.BlockBurnEvent.class, f -> {
            Block block = f.block();
            return block == null ? null : new org.bukkit.event.block.BlockBurnEvent(block, f.world().getBlockAt(
                (int) f.whole("igniter_x"), (int) f.whole("igniter_y"), (int) f.whole("igniter_z")));
        });
        EventBridge.map("block.fade", org.bukkit.event.block.LeavesDecayEvent.class, f -> {
            Block block = f.block();
            Material from = Material.matchMaterial(f.text("block"));
            return block == null || from == null || !org.bukkit.Tag.LEAVES.isTagged(from) ? null
                : new org.bukkit.event.block.LeavesDecayEvent(block);
        });
        EventBridge.map("block.fertilize", org.bukkit.event.block.BlockFertilizeEvent.class, f -> {
            Block block = f.block();
            return block == null ? null : new org.bukkit.event.block.BlockFertilizeEvent(block, f.player(),
                new ArrayList<>());
        });
        EventBridge.map("block.from_to", org.bukkit.event.block.BlockFromToEvent.class, f -> {
            Block block = f.block();
            return block == null ? null : new org.bukkit.event.block.BlockFromToEvent(block, f.world().getBlockAt(
                (int) f.whole("to_x"), (int) f.whole("to_y"), (int) f.whole("to_z")));
        });
        EventBridge.map("block.physics", org.bukkit.event.block.BlockPhysicsEvent.class, f -> {
            Block block = f.block();
            return block == null ? null : new org.bukkit.event.block.BlockPhysicsEvent(block, block.getBlockData(),
                block);
        });

        EventBridge.map("block.drops", org.bukkit.event.block.BlockDropItemEvent.class, f -> {
            Block block = f.block();
            BasaltWorld world = f.world();
            Player who = f.player();
            if (block == null || world == null || who == null) {
                return null;
            }
            List<org.bukkit.entity.Item> items = new ArrayList<>();
            for (String line : f.text("drops").split("\n")) {
                int bar = line.indexOf('|');
                ItemStack stack = bar < 0 ? null : PlayerEvents.stackOf(line.substring(0, bar),
                    Integer.parseInt(line.substring(bar + 1)));
                if (stack != null) {
                    Entity item = org.basaltmc.bukkit.entity.EntityCore.template(world,
                        org.bukkit.entity.EntityType.ITEM, block.getX() + 0.5, block.getY() + 0.5, block.getZ() + 0.5,
                        0f, 0f);
                    ((org.basaltmc.bukkit.entity.Cored) item).core().stageNbt("Item",
                        org.basaltmc.bukkit.entity.EntityItems.toNbt(stack));
                    items.add((org.bukkit.entity.Item) item);
                }
            }
            return new org.bukkit.event.block.BlockDropItemEvent(block, block.getState(), who, items);
        }, (e, f) -> {
            StringBuilder lines = new StringBuilder();
            for (org.bukkit.entity.Item item : ((org.bukkit.event.block.BlockDropItemEvent) e).getItems()) {
                ItemStack stack = item.getItemStack();
                if (stack != null && !stack.getType().isAir() && stack.getAmount() > 0) {
                    lines.append(lines.length() == 0 ? "" : "\n").append(stack.getType().getKey()).append('|')
                        .append(stack.getAmount());
                }
            }
            f.put("drops", lines.toString());
        });
        EventBridge.map("block.dispense", org.bukkit.event.block.BlockDispenseEvent.class, f -> {
            Block block = f.block();
            ItemStack stack = PlayerEvents.stackOf(f.text("item"), 1);
            org.bukkit.block.BlockFace face = PlayerEvents.face(f.text("face"));
            return block == null ? null : new org.bukkit.event.block.BlockDispenseEvent(block,
                stack == null ? new ItemStack(Material.AIR) : stack,
                new Vector(face.getModX(), face.getModY() + 0.1, face.getModZ()));
        });
        EventBridge.map("block.piston", org.bukkit.event.block.BlockPistonExtendEvent.class, f -> {
            Block block = f.block();
            return block == null || !f.flag("extending") ? null : new org.bukkit.event.block.BlockPistonExtendEvent(
                block, moved(block, f), PlayerEvents.face(f.text("direction")));
        });
        EventBridge.map("block.piston", org.bukkit.event.block.BlockPistonRetractEvent.class, f -> {
            Block block = f.block();
            return block == null || f.flag("extending") ? null : new org.bukkit.event.block.BlockPistonRetractEvent(
                block, moved(block, f), PlayerEvents.face(f.text("direction")));
        });
        EventBridge.map("block.explode", org.bukkit.event.block.BlockExplodeEvent.class, f -> {
            Block block = f.block();
            return block == null || f.whole("entity_id") > 0 ? null : new org.bukkit.event.block.BlockExplodeEvent(
                block, block.getState(), new ArrayList<>(), 1.0f, ExplosionResult.DESTROY);
        });
    }

    private static List<Block> moved(Block piston, EventBridge.Fields f) {
        List<Block> blocks = new ArrayList<>();
        BlockFace direction = PlayerEvents.face(f.text("direction"));
        for (int step = 1; step <= f.whole("moved"); step++) {
            blocks.add(piston.getRelative(direction, step));
        }
        return blocks;
    }
}
