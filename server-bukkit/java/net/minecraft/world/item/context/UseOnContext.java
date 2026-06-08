package net.minecraft.world.item.context;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.phys.BlockHitResult;
public class UseOnContext {
    private final Player player;
    private final InteractionHand hand;
    private final BlockHitResult hit;

    public UseOnContext(Player player, InteractionHand interactionHand, BlockHitResult blockHitResult) {
        this.player = player;
        this.hand = interactionHand;
        this.hit = blockHitResult;
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
    public ServerLevel getLevel() {
        return player.level();
    }
    public BlockHitResult getHitResult() {
        return hit;
    }
}
