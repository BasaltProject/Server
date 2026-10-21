package org.basaltmc.hello;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Tag;
import org.bukkit.World;
import org.bukkit.block.Biome;
import org.bukkit.block.Block;
import org.bukkit.block.BlockFace;
import org.bukkit.block.data.BlockData;
import org.bukkit.block.data.type.Stairs;
import org.bukkit.command.Command;
import org.bukkit.command.CommandExecutor;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntityType;
import org.bukkit.entity.Player;
import org.bukkit.inventory.ItemStack;
import org.bukkit.plugin.java.JavaPlugin;

/** Runs a battery of world-facing API calls and logs one PROBE line for each. */
final class WorldProbe implements CommandExecutor {
    private final JavaPlugin plugin;
    private int failures;

    WorldProbe(JavaPlugin plugin) {
        this.plugin = plugin;
    }

    private void check(String name, Callable<Object> body) {
        try {
            Object result = body.call();
            plugin.getLogger().info("PROBE " + name + ": " + result);
        } catch (Throwable t) {
            failures++;
            plugin.getLogger().warning("PROBE " + name + ": FAIL " + t + (t.getCause() == null ? "" : " cause " + t.getCause()));
            for (StackTraceElement frame : t.getStackTrace()) {
                plugin.getLogger().warning("    at " + frame);
                if (frame.getClassName().startsWith("org.basaltmc.hello")) {
                    break;
                }
            }
        }
    }

    @Override
    public boolean onCommand(CommandSender sender, Command command, String label, String[] args) {
        failures = 0;
        World world = Bukkit.getWorlds().get(0);
        int bx = 1000;
        int bz = 1000;
        check("worlds", () -> {
            List<String> names = new ArrayList<>();
            for (World each : Bukkit.getWorlds()) {
                names.add(each.getName() + "/" + each.getEnvironment() + "/seed=" + each.getSeed() + "/y="
                    + each.getMinHeight() + ".." + each.getMaxHeight());
            }
            return names + " spawn=" + world.getSpawnLocation() + " difficulty=" + world.getDifficulty();
        });
        check("blockdata.parse", () -> {
            BlockData data = Bukkit.createBlockData("minecraft:oak_stairs[facing=east,half=top]");
            Stairs stairs = (Stairs) data;
            String before = data.getAsString();
            stairs.setFacing(BlockFace.WEST);
            return before + " -> " + stairs.getAsString() + " facing=" + stairs.getFacing() + " half=" + stairs.getHalf()
                + " material=" + stairs.getMaterial();
        });
        check("material.helpers", () -> "STONE block=" + Material.STONE.isBlock() + " solid=" + Material.STONE.isSolid()
            + " air=" + Material.AIR.isAir() + " item=" + Material.DIAMOND.isItem() + " diamondIsBlock="
            + Material.DIAMOND.isBlock() + " water solid=" + Material.WATER.isSolid());
        check("registries", () -> "blocks=" + Registry.BLOCK.size() + " biomes=" + Registry.BIOME.size()
            + " plains=" + Registry.BIOME.get(NamespacedKey.minecraft("plains")) + " woolTag="
            + Tag.WOOL.isTagged(Material.WHITE_WOOL) + "/" + Tag.WOOL.getValues().size());
        check("chunk.load", () -> {
            org.bukkit.Chunk chunk = world.getChunkAt(bx >> 4, bz >> 4);
            return chunk + " loaded=" + chunk.isLoaded() + " entities=" + chunk.getEntities().length
                + " world.loadedChunks=" + world.getLoadedChunks().length;
        });
        check("block.setType", () -> {
            Block block = world.getBlockAt(bx, 200, bz);
            block.setType(Material.STONE);
            String after = block.getType() + " solid=" + block.isSolid();
            block.setBlockData(Bukkit.createBlockData("minecraft:oak_stairs[facing=south]"));
            String stairs = block.getBlockData().getAsString();
            block.setType(Material.AIR);
            return after + " | " + stairs + " | now " + block.getType() + " empty=" + block.isEmpty();
        });
        check("block.state", () -> {
            Block block = world.getBlockAt(bx, 200, bz);
            org.bukkit.block.BlockState state = block.getState();
            state.setType(Material.GOLD_BLOCK);
            state.update(true);
            String result = block.getType().toString();
            block.setType(Material.AIR);
            return "state.update -> " + result;
        });
        check("block.metadata", () -> {
            Block block = world.getBlockAt(bx, 201, bz);
            block.setMetadata("probe", new org.bukkit.metadata.FixedMetadataValue(plugin, 42));
            return block.hasMetadata("probe") + "/" + block.getMetadata("probe").get(0).asInt();
        });
        check("bulk.section", () -> {
            int[] states = new int[4096];
            int stone = org.basaltmc.bukkit.WorldNatives.stateId("minecraft:stone");
            java.util.Arrays.fill(states, stone);
            long t0 = System.nanoTime();
            boolean wrote = org.basaltmc.bukkit.WorldNatives.writeSection(world.getName(), bx >> 4, 12, bz >> 4, states, false);
            long t1 = System.nanoTime();
            int[] back = new int[4096];
            boolean read = org.basaltmc.bukkit.WorldNatives.readSection(world.getName(), bx >> 4, 12, bz >> 4, false, back);
            long t2 = System.nanoTime();
            boolean same = java.util.Arrays.equals(states, back);
            java.util.Arrays.fill(states, org.basaltmc.bukkit.WorldNatives.stateId("minecraft:air"));
            org.basaltmc.bukkit.WorldNatives.writeSection(world.getName(), bx >> 4, 12, bz >> 4, states, false);
            return "write=" + wrote + " " + (t1 - t0) / 1000 + "us read=" + read + " " + (t2 - t1) / 1000 + "us same=" + same;
        });
        check("bulk.setType4096", () -> {
            long t0 = System.nanoTime();
            for (int i = 0; i < 4096; i++) {
                world.getBlockAt(bx + (i & 15), 220 + (i >> 8), bz + ((i >> 4) & 15)).setType(Material.DIRT);
            }
            long t1 = System.nanoTime();
            for (int i = 0; i < 4096; i++) {
                world.getBlockAt(bx + (i & 15), 220 + (i >> 8), bz + ((i >> 4) & 15)).setType(Material.AIR);
            }
            return "4096 Block.setType in " + (t1 - t0) / 1000 + "us";
        });
        check("highest", () -> world.getHighestBlockAt(bx, bz) + " y=" + world.getHighestBlockYAt(bx, bz));
        check("biome.class", () -> Class.forName("org.bukkit.block.Biome").getName() + " " + Biome.DESERT);
        check("biome", () -> {
            Biome before = world.getBiome(bx, 100, bz);
            world.setBiome(bx, 100, bz, Biome.DESERT);
            Biome after = world.getBiome(bx, 100, bz);
            world.setBiome(bx, 100, bz, before);
            return before + " -> " + after;
        });
        check("world.time", () -> {
            long was = world.getFullTime();
            world.setFullTime(6000);
            long now = world.getTime();
            world.setFullTime(was);
            return was + " -> " + now + " storm=" + world.hasStorm();
        });
        check("world.gamerule", () -> {
            String[] rules = world.getGameRules();
            String first = rules.length == 0 ? "none" : rules[0];
            return rules.length + " rules, " + first + "=" + world.getGameRuleValue(first);
        });
        check("entity.spawn", () -> {
            Location where = new Location(world, bx + 0.5, 210, bz + 0.5);
            Entity cow = world.spawnEntity(where, EntityType.COW);
            String info = cow == null ? "null" : cow.getType() + " id=" + cow.getEntityId() + " loc=" + cow.getLocation()
                + " health=" + ((org.bukkit.entity.LivingEntity) cow).getHealth() + " nearby=" + cow.getNearbyEntities(5, 5, 5).size()
                + " inWorld=" + world.getEntities().contains(cow) + " byUuid=" + (world.getEntity(cow.getUniqueId()) != null);
            if (cow != null) {
                cow.setVelocity(new org.bukkit.util.Vector(0, 0.5, 0));
                cow.teleport(where.clone().add(2, 0, 0));
                info += " moved=" + cow.getLocation().getX();
                cow.remove();
                info += " removedDead=" + cow.isDead();
            }
            return info;
        });
        if (sender instanceof Player player) {
            check("player.state", () -> player.getName() + " loc=" + player.getLocation() + " health=" + player.getHealth()
                + " food=" + player.getFoodLevel() + " gm=" + player.getGameMode() + " op=" + player.isOp()
                + " addr=" + player.getAddress() + " ping=" + player.getPing() + " id=" + player.getEntityId());
            check("player.inventory", () -> {
                player.getInventory().setItem(3, new ItemStack(Material.DIAMOND, 7));
                ItemStack back = player.getInventory().getItem(3);
                return back + " main=" + player.getInventory().getItemInMainHand() + " held=" + player.getInventory().getHeldItemSlot();
            });
            check("player.sendBlockChange", () -> {
                Location at = player.getLocation().add(0, 3, 0);
                player.sendBlockChange(at, Material.GLASS.createBlockData());
                return "sent at " + at.getBlockY();
            });
            check("player.title", () -> {
                player.sendTitle("Probe", "world API", 5, 40, 5);
                player.sendActionBar(net.kyori.adventure.text.Component.text("probe action bar"));
                return "sent";
            });
            check("player.command", () -> player.performCommand("say probe from " + player.getName()));
            check("player.teleport", () -> {
                Location target = player.getLocation().add(3, 0, 0);
                target.setYaw(90f);
                boolean moved = player.teleport(target);
                return moved + " yaw=" + player.getLocation().getYaw();
            });
            check("world.players", () -> world.getPlayers().size() + " in " + world.getName());
        }
        sender.sendMessage("probe finished, failures=" + failures);
        plugin.getLogger().info("PROBE done failures=" + failures);
        return true;
    }
}
