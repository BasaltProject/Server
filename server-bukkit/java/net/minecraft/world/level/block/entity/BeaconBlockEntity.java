package net.minecraft.world.level.block.entity;

import net.minecraft.core.BlockPos;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.Nameable;
import net.minecraft.world.level.Level;
import org.basaltmc.bukkit.Unsupported;
public class BeaconBlockEntity extends BlockEntity implements MenuProvider, Nameable, BeaconBeamOwner {
    protected BeaconBlockEntity() {
    }
    public static void playSound(Level level, BlockPos worldPosition, SoundEvent event) {
        throw Unsupported.hit("net.minecraft.world.level.block.entity.BeaconBlockEntity.playSound");
    }
}
