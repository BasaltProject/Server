package org.bukkit.craftbukkit.block.data;

import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import org.basaltmc.bukkit.BasaltBlockData;
import org.bukkit.Color;
import org.bukkit.Location;
import org.bukkit.Material;
import org.bukkit.SoundGroup;
import org.bukkit.block.BlockFace;
import org.bukkit.block.BlockSupport;
import org.bukkit.block.PistonMoveReaction;
import org.bukkit.block.data.BlockData;
import org.bukkit.block.structure.Mirror;
import org.bukkit.block.structure.StructureRotation;
import org.bukkit.inventory.ItemStack;
import org.bukkit.util.VoxelShape;

public class CraftBlockData implements BlockData {
    private final BlockData inner;

    protected CraftBlockData(BlockData inner) {
        this.inner = inner;
    }

    public static CraftBlockData wrap(BlockData inner) {
        return CraftBlockDataTypes.make(inner);
    }

    public static CraftBlockData createData(BlockState state) {
        return BasaltBlockData.of(state.stateId()) instanceof CraftBlockData data ? data : null;
    }

    public static CraftBlockData fromData(BlockState state) {
        return createData(state);
    }

    public static CraftBlockData newData(Material material, String data) {
        BlockData parsed = BasaltBlockData.parse(data == null || data.isEmpty() ? material.getKey().toString()
            : data.startsWith("[") ? material.getKey() + data : data);
        return parsed instanceof CraftBlockData made ? made : null;
    }

    public final BlockData inner0() {
        return inner;
    }

    protected final BlockData inner() {
        return inner;
    }

    public int stateId() {
        return BasaltBlockData.stateOf(inner);
    }

    public BlockState getState() {
        return Block.stateById(stateId());
    }

    @Override
    public Material getMaterial() {
        return inner.getMaterial();
    }

    @Override
    public String getAsString() {
        return inner.getAsString();
    }

    @Override
    public String getAsString(boolean hideUnspecified) {
        return inner.getAsString(hideUnspecified);
    }

    @Override
    public BlockData merge(BlockData data) {
        return inner.merge(data);
    }

    @Override
    public boolean matches(BlockData data) {
        return inner.matches(data);
    }

    @Override
    public BlockData clone() {
        return BasaltBlockData.of(stateId());
    }

    @Override
    public SoundGroup getSoundGroup() {
        return inner.getSoundGroup();
    }

    @Override
    public int getLightEmission() {
        return inner.getLightEmission();
    }

    @Override
    public boolean isOccluding() {
        return inner.isOccluding();
    }

    @Override
    public boolean requiresCorrectToolForDrops() {
        return inner.requiresCorrectToolForDrops();
    }

    @Override
    public boolean isPreferredTool(ItemStack tool) {
        return inner.isPreferredTool(tool);
    }

    @Override
    public PistonMoveReaction getPistonMoveReaction() {
        return inner.getPistonMoveReaction();
    }

    @Override
    public boolean isSupported(org.bukkit.block.Block block) {
        return inner.isSupported(block);
    }

    @Override
    public boolean isSupported(Location location) {
        return inner.isSupported(location);
    }

    @Override
    public boolean isFaceSturdy(BlockFace face, BlockSupport support) {
        return inner.isFaceSturdy(face, support);
    }

    @Override
    public VoxelShape getCollisionShape(Location location) {
        return inner.getCollisionShape(location);
    }

    @Override
    public Color getMapColor() {
        return inner.getMapColor();
    }

    @Override
    public Material getPlacementMaterial() {
        return inner.getPlacementMaterial();
    }

    @Override
    public void rotate(StructureRotation rotation) {
        inner.rotate(rotation);
    }

    @Override
    public void mirror(Mirror mirror) {
        inner.mirror(mirror);
    }

    @Override
    public void copyTo(BlockData target) {
        inner.copyTo(target);
    }

    @Override
    public org.bukkit.block.BlockState createBlockState() {
        return inner.createBlockState();
    }

    @Override
    public float getDestroySpeed(ItemStack tool, boolean considerEnchants) {
        return inner.getDestroySpeed(tool, considerEnchants);
    }

    @Override
    public boolean isRandomlyTicked() {
        return inner.isRandomlyTicked();
    }

    @Override
    public boolean isReplaceable() {
        return inner.isReplaceable();
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof CraftBlockData data && data.stateId() == stateId();
    }

    @Override
    public int hashCode() {
        return stateId();
    }

    @Override
    public String toString() {
        return "CraftBlockData{" + getAsString() + "}";
    }
}
