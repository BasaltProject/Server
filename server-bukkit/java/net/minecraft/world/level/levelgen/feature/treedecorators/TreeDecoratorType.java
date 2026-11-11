// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.treedecorators;
import com.mojang.serialization.MapCodec;
public class TreeDecoratorType<P extends TreeDecorator> {
    private final MapCodec<P> codec;
    public static final TreeDecoratorType<TrunkVineDecorator> TRUNK_VINE = new TreeDecoratorType<>(TrunkVineDecorator.CODEC);
    public static final TreeDecoratorType<LeaveVineDecorator> LEAVE_VINE = new TreeDecoratorType<>(LeaveVineDecorator.CODEC);
    public static final TreeDecoratorType<PaleMossDecorator> PALE_MOSS = new TreeDecoratorType<>(PaleMossDecorator.CODEC);
    public static final TreeDecoratorType<CreakingHeartDecorator> CREAKING_HEART = new TreeDecoratorType<>(CreakingHeartDecorator.CODEC);
    public static final TreeDecoratorType<CocoaDecorator> COCOA = new TreeDecoratorType<>(CocoaDecorator.CODEC);
    public static final TreeDecoratorType<ShelfMushroomDecorator> SHELF_MUSHROOM = new TreeDecoratorType<>(ShelfMushroomDecorator.CODEC);
    public static final TreeDecoratorType<BeehiveDecorator> BEEHIVE = new TreeDecoratorType<>(BeehiveDecorator.CODEC);
    public static final TreeDecoratorType<AlterGroundDecorator> ALTER_GROUND = new TreeDecoratorType<>(AlterGroundDecorator.CODEC);
    public static final TreeDecoratorType<AttachedToLeavesDecorator> ATTACHED_TO_LEAVES = new TreeDecoratorType<>(AttachedToLeavesDecorator.CODEC);
    public static final TreeDecoratorType<PlaceOnGroundDecorator> PLACE_ON_GROUND = new TreeDecoratorType<>(PlaceOnGroundDecorator.CODEC);
    public static final TreeDecoratorType<AttachedToLogsDecorator> ATTACHED_TO_LOGS = new TreeDecoratorType<>(AttachedToLogsDecorator.CODEC);
    private TreeDecoratorType(MapCodec<P> codec) {
        this.codec = codec;
    }
    public MapCodec<P> codec() {
        return codec;
    }
}
