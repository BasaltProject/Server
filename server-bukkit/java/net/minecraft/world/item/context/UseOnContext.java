// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.context;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

public class UseOnContext {
    private final Player player;
    private final InteractionHand hand;
    private final BlockHitResult hit;
    private final Level level;
    private final ItemStack stack;

    public UseOnContext(Player player, InteractionHand interactionHand, BlockHitResult blockHitResult) {
        this(player.level(), player, interactionHand, player.getItemInHand(interactionHand), blockHitResult);
    }

    protected UseOnContext(Level level, Player player, InteractionHand hand, ItemStack itemStack, BlockHitResult hitResult) {
        this.level = level;
        this.player = player;
        this.hand = hand;
        this.stack = itemStack;
        this.hit = hitResult;
    }

    public BlockPos getClickedPos() {
        return hit.getBlockPos();
    }

    public Direction getClickedFace() {
        return hit.getDirection();
    }

    public InteractionHand getHand() {
        return hand;
    }

    public Player getPlayer() {
        return player;
    }

    public Level getLevel() {
        return level;
    }

    public BlockHitResult getHitResult() {
        return hit;
    }

    public Vec3 getClickLocation() {
        return hit.getLocation();
    }

    public boolean isInside() {
        return hit.isInside();
    }

    public ItemStack getItemInHand() {
        return stack;
    }

    public Direction getHorizontalDirection() {
        return player == null ? Direction.NORTH : player.getDirection();
    }

    public boolean isSecondaryUseActive() {
        return player != null && player.isSecondaryUseActive();
    }

    public float getRotation() {
        return player == null ? 0.0F : player.getYRot();
    }
}
