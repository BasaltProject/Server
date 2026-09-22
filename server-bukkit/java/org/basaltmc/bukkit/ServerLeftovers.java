package org.basaltmc.bukkit;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.World;
import org.bukkit.generator.structure.Structure;
import org.bukkit.generator.structure.StructureType;
import org.bukkit.inventory.ItemStack;
import org.bukkit.map.MapCursor;
import org.bukkit.potion.PotionBrewer;
import org.bukkit.potion.PotionEffect;
import org.bukkit.potion.PotionType;
import org.bukkit.structure.StructureManager;
import org.bukkit.util.CachedServerIcon;

public abstract class ServerLeftovers extends ServerExtras {
    private final BasaltStructureManager structures = new BasaltStructureManager();

    @Override
    public StructureManager getStructureManager() {
        return structures;
    }

    private static ItemStack explorerMap(World world, Location origin, List<Structure> candidates, MapCursor.Type type,
            int radius, boolean findUnexplored) {
        if (candidates.isEmpty()) {
            return null;
        }
        List<String> names = new ArrayList<>();
        for (Structure candidate : candidates) {
            names.add(candidate.getKey().toString());
        }
        byte[] made = LeftoverNatives.explorerMap(world.getName(), origin.getX(), origin.getY(), origin.getZ(),
            names.toArray(new String[0]), type.getKey().toString(), 2, Math.max(radius, 1), findUnexplored);
        if (made == null) {
            return null;
        }
        return new BasaltItemStack(Material.FILLED_MAP, 1, Nbt.read(made));
    }

    @Override
    public ItemStack createExplorerMap(World world, Location location, Structure structure, MapCursor.Type type,
            int radius, boolean findUnexplored) {
        return explorerMap(world, location, List.of(structure), type, radius, findUnexplored);
    }

    @Override
    public ItemStack createExplorerMap(World world, Location location, StructureType structureType,
            MapCursor.Type type, int radius, boolean findUnexplored) {
        return explorerMap(world, location, StructureKinds.structuresOf(structureType), type, radius, findUnexplored);
    }

    @Override
    public ItemStack createExplorerMap(World world, Location location, org.bukkit.StructureType structureType,
            int radius, boolean findUnexplored) {
        String wanted = structureType.getName().replace("_", "");
        List<Structure> candidates = new ArrayList<>();
        for (Structure structure : org.bukkit.Registry.STRUCTURE) {
            if (structure.getKey().getKey().replace("_", "").startsWith(wanted)) {
                candidates.add(structure);
            }
        }
        return explorerMap(world, location, candidates, MapCursor.Type.RED_X, radius, findUnexplored);
    }

    @Override
    public boolean isOwnedByCurrentRegion(World world, int minChunkX, int minChunkZ, int maxChunkX, int maxChunkZ) {
        return isPrimaryThread();
    }

    @Override
    public PotionBrewer getPotionBrewer() {
        return new PotionBrewer() {
            @Override
            public Collection<PotionEffect> getEffects(PotionType type, boolean upgraded, boolean extended) {
                return type.getPotionEffects();
            }

            @Override
            public void resetPotionMixes() {
                InventoryNatives.resetBrewing();
            }
        };
    }

    @Override
    public CachedServerIcon loadServerIcon(BufferedImage image) throws Exception {
        if (image.getWidth() != 64 || image.getHeight() != 64) {
            throw new IllegalArgumentException("Must be 64 by 64");
        }
        java.io.ByteArrayOutputStream png = new java.io.ByteArrayOutputStream();
        javax.imageio.ImageIO.write(image, "PNG", png);
        return ServerIcon.of(png.toByteArray());
    }

    @Override
    public void reloadData() {
        RestNatives.reloadData();
        dropDataCaches();
    }

    private static void dropDataCaches() {
        org.basaltmc.bukkit.item.TagSets.invalidate();
        org.basaltmc.bukkit.item.ItemData.invalidate();
        org.basaltmc.bukkit.item.ItemDefaults.invalidate();
        BasaltTag.invalidate();
    }

    @Override
    public void updateResources() {
        RestNatives.reloadData();
        dropDataCaches();
    }
}
