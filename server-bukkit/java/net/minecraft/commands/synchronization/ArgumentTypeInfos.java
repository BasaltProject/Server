// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.synchronization;

import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.arguments.BoolArgumentType;
import com.mojang.brigadier.arguments.DoubleArgumentType;
import com.mojang.brigadier.arguments.FloatArgumentType;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.LongArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.commands.arguments.AngleArgument;
import net.minecraft.commands.arguments.CompoundTagArgument;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.commands.arguments.EntityArgument;
import net.minecraft.commands.arguments.GameModeArgument;
import net.minecraft.commands.arguments.HexColorArgument;
import net.minecraft.commands.arguments.IdentifierArgument;
import net.minecraft.commands.arguments.MessageArgument;
import net.minecraft.commands.arguments.NbtPathArgument;
import net.minecraft.commands.arguments.ObjectiveArgument;
import net.minecraft.commands.arguments.ObjectiveCriteriaArgument;
import net.minecraft.commands.arguments.OperationArgument;
import net.minecraft.commands.arguments.ScoreHolderArgument;
import net.minecraft.commands.arguments.TeamArgument;
import net.minecraft.commands.arguments.RangeArgument;
import net.minecraft.commands.arguments.ResourceArgument;
import net.minecraft.commands.arguments.ResourceOrTagArgument;
import net.minecraft.commands.arguments.ResourceOrTagKeyArgument;
import net.minecraft.commands.arguments.ResourceKeyArgument;
import net.minecraft.commands.arguments.ScoreboardSlotArgument;
import net.minecraft.commands.arguments.TeamColorArgument;
import net.minecraft.commands.arguments.TimeArgument;
import net.minecraft.commands.arguments.UuidArgument;
import net.minecraft.commands.arguments.blocks.BlockStateArgument;
import net.minecraft.commands.arguments.item.ItemArgument;
import net.minecraft.commands.arguments.coordinates.BlockPosArgument;
import net.minecraft.commands.arguments.coordinates.ColumnPosArgument;
import net.minecraft.commands.arguments.coordinates.RotationArgument;
import net.minecraft.commands.arguments.coordinates.Vec2Argument;
import net.minecraft.commands.arguments.coordinates.Vec3Argument;
import net.minecraft.commands.synchronization.brigadier.DoubleArgumentInfo;
import net.minecraft.commands.synchronization.brigadier.FloatArgumentInfo;
import net.minecraft.commands.synchronization.brigadier.IntegerArgumentInfo;
import net.minecraft.commands.synchronization.brigadier.LongArgumentInfo;
import net.minecraft.commands.synchronization.brigadier.StringArgumentSerializer;
import net.minecraft.core.Registry;

public class ArgumentTypeInfos {
    private static final ArgumentTypeInfo<?, ?>[] kById = new ArgumentTypeInfo<?, ?>[64];
    private static final Map<Class<?>, ArgumentTypeInfo<?, ?>> kByClass = new HashMap<>();

    static {
        add(0, BoolArgumentType.class, SingletonArgumentInfo.contextFree(BoolArgumentType::bool));
        add(1, FloatArgumentType.class, new FloatArgumentInfo());
        add(2, DoubleArgumentType.class, new DoubleArgumentInfo());
        add(3, IntegerArgumentType.class, new IntegerArgumentInfo());
        add(4, LongArgumentType.class, new LongArgumentInfo());
        add(5, StringArgumentType.class, new StringArgumentSerializer());
        add(6, EntityArgument.class, new EntityArgument.Info());
        add(8, BlockPosArgument.class, SingletonArgumentInfo.contextFree(BlockPosArgument::blockPos));
        add(9, ColumnPosArgument.class, SingletonArgumentInfo.contextFree(ColumnPosArgument::columnPos));
        add(10, Vec3Argument.class, SingletonArgumentInfo.contextFree(Vec3Argument::vec3));
        add(11, Vec2Argument.class, SingletonArgumentInfo.contextFree(Vec2Argument::vec2));
        add(12, BlockStateArgument.class, SingletonArgumentInfo.contextAware(BlockStateArgument::block));
        add(14, ItemArgument.class, SingletonArgumentInfo.contextAware(ItemArgument::item));
        add(16, TeamColorArgument.class, SingletonArgumentInfo.contextFree(TeamColorArgument::teamColor));
        add(17, HexColorArgument.class, SingletonArgumentInfo.contextFree(HexColorArgument::hexColor));
        add(20, MessageArgument.class, SingletonArgumentInfo.contextFree(MessageArgument::message));
        add(21, CompoundTagArgument.class, SingletonArgumentInfo.contextFree(CompoundTagArgument::compoundTag));
        add(23, NbtPathArgument.class, SingletonArgumentInfo.contextFree(NbtPathArgument::nbtPath));
        add(24, ObjectiveArgument.class, SingletonArgumentInfo.contextFree(ObjectiveArgument::objective));
        add(25, ObjectiveCriteriaArgument.class, SingletonArgumentInfo.contextFree(ObjectiveCriteriaArgument::criteria));
        add(31, ScoreHolderArgument.class, new ScoreHolderArgument.Info());
        add(33, TeamArgument.class, SingletonArgumentInfo.contextFree(TeamArgument::team));
        add(26, OperationArgument.class, SingletonArgumentInfo.contextFree(OperationArgument::operation));
        add(28, AngleArgument.class, SingletonArgumentInfo.contextFree(AngleArgument::angle));
        add(29, RotationArgument.class, SingletonArgumentInfo.contextFree(RotationArgument::rotation));
        add(30, ScoreboardSlotArgument.class, SingletonArgumentInfo.contextFree(ScoreboardSlotArgument::displaySlot));
        add(36, IdentifierArgument.class, SingletonArgumentInfo.contextFree(IdentifierArgument::id));
        add(38, EntityAnchorArgument.class, SingletonArgumentInfo.contextFree(EntityAnchorArgument::anchor));
        add(39, RangeArgument.Ints.class, SingletonArgumentInfo.contextFree(RangeArgument::intRange));
        add(40, RangeArgument.Floats.class, SingletonArgumentInfo.contextFree(RangeArgument::floatRange));
        add(42, GameModeArgument.class, SingletonArgumentInfo.contextFree(GameModeArgument::gameMode));
        add(43, TimeArgument.class, new TimeArgument.Info());
        add(44, ResourceOrTagArgument.class, new ResourceOrTagArgument.Info<>());
        add(45, ResourceOrTagKeyArgument.class, new ResourceOrTagKeyArgument.Info<>());
        add(46, ResourceArgument.class, new ResourceArgument.Info<>());
        add(47, ResourceKeyArgument.class, new ResourceKeyArgument.Info<>());
        add(61, UuidArgument.class, SingletonArgumentInfo.contextFree(UuidArgument::uuid));
    }

    private static void add(int id, Class<?> type, ArgumentTypeInfo<?, ?> info) {
        kById[id] = info;
        kByClass.put(type, info);
    }

    public static ArgumentTypeInfo<?, ?> bootstrap(Registry<ArgumentTypeInfo<?, ?>> registry) {
        return kById[61];
    }

    public static boolean isClassRecognized(Class<?> cls) {
        return kByClass.containsKey(cls);
    }

    @SuppressWarnings("unchecked")
    public static <A extends ArgumentType<?>> ArgumentTypeInfo<A, ?> byClass(A argumentType) {
        ArgumentTypeInfo<?, ?> info = kByClass.get(argumentType.getClass());
        if (info == null) {
            throw new IllegalArgumentException("Unrecognized argument type " + argumentType);
        }
        return (ArgumentTypeInfo<A, ?>) info;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <A extends ArgumentType<?>> ArgumentTypeInfo.Template<A> unpack(A argumentType) {
        return (ArgumentTypeInfo.Template<A>) ((ArgumentTypeInfo) byClass(argumentType)).unpack(argumentType);
    }

    public static int idOf(ArgumentTypeInfo<?, ?> info) {
        for (int id = 0; id < kById.length; id++) {
            if (kById[id] == info) {
                return id;
            }
        }
        throw new IllegalArgumentException("Argument type info is not registered: " + info);
    }

    public static ArgumentTypeInfo<?, ?> byId(int id) {
        return id >= 0 && id < kById.length ? kById[id] : null;
    }
}
