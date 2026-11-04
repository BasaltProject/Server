// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import io.netty.handler.codec.DecoderException;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.world.entity.PositionPath;
import net.minecraft.world.entity.PositionStep;
import net.minecraft.world.phys.Vec3;

public interface VecDelta {
    VecDelta ZERO = new Linear((short) 0, (short) 0, (short) 0);

    int stepCount();

    boolean hasDeltaX();

    boolean hasDeltaZ();

    PositionPath decode(VecDeltaCodec positionCodec);

    static VecDelta read(FriendlyByteBuf input, int stepCount) {
        if (stepCount <= 0) {
            return new Linear(input.readShort(), input.readShort(), input.readShort());
        }
        int maxSteps = input.readableBytes() / Stepped.kMinBytesPerStep;
        if (stepCount > maxSteps) {
            throw new DecoderException("VecDelta with size " + stepCount + " is bigger than allowed " + maxSteps);
        }
        List<Stepped.DeltaStep> steps = new ArrayList<>(stepCount);
        for (int i = 0; i < stepCount; i++) {
            int ticks = input.readVarInt();
            steps.add(new Stepped.DeltaStep(input.readShort(), input.readShort(), input.readShort(), ticks));
        }
        return new Stepped(steps);
    }

    static void write(FriendlyByteBuf output, VecDelta delta) {
        if (delta instanceof Linear linear) {
            output.writeShort(linear.xa());
            output.writeShort(linear.ya());
            output.writeShort(linear.za());
        } else if (delta instanceof Stepped stepped) {
            for (Stepped.DeltaStep step : stepped.steps()) {
                output.writeVarInt(step.ticks());
                output.writeShort(step.xa());
                output.writeShort(step.ya());
                output.writeShort(step.za());
            }
        } else {
            throw new IllegalArgumentException("Unknown VecDelta " + delta);
        }
    }

    public static record Linear(short xa, short ya, short za) implements VecDelta {
        public int stepCount() {
            return 0;
        }

        public boolean hasDeltaX() {
            return xa != 0;
        }

        public boolean hasDeltaZ() {
            return za != 0;
        }

        public PositionPath decode(VecDeltaCodec positionCodec) {
            return PositionPath.of(positionCodec.decode(xa, ya, za));
        }
    }

    public static record Stepped(List<DeltaStep> steps) implements VecDelta {
        static final int kMinBytesPerStep = 7;

        public int stepCount() {
            return steps.size();
        }

        public boolean hasDeltaX() {
            return steps.stream().anyMatch(step -> step.xa() != 0);
        }

        public boolean hasDeltaZ() {
            return steps.stream().anyMatch(step -> step.za() != 0);
        }

        public PositionPath decode(VecDeltaCodec positionCodec) {
            if (steps.isEmpty()) {
                return PositionPath.of(positionCodec.getBase());
            }
            List<PositionStep> output = new ArrayList<>(steps.size());
            Vec3 originalBase = positionCodec.getBase();
            for (DeltaStep step : steps) {
                Vec3 pos = positionCodec.decode(step.xa(), step.ya(), step.za());
                output.add(new PositionStep(pos, step.ticks()));
                positionCodec.setBase(pos);
            }
            positionCodec.setBase(originalBase);
            return PositionPath.stepped(output);
        }

        public static Stepped tryEncode(VecDeltaCodec positionCodec, List<PositionStep> steps) {
            if (steps.isEmpty()) {
                return new Stepped(List.of());
            }
            List<DeltaStep> output = new ArrayList<>(steps.size());
            Vec3 originalBase = positionCodec.getBase();
            for (PositionStep step : steps) {
                Vec3 pos = step.position();
                long xa = positionCodec.encodeX(pos);
                long ya = positionCodec.encodeY(pos);
                long za = positionCodec.encodeZ(pos);
                if (VecDeltaCodec.isDeltaTooBig(xa, ya, za)) {
                    positionCodec.setBase(originalBase);
                    return null;
                }
                output.add(new DeltaStep((short) xa, (short) ya, (short) za, step.tickOffset()));
                positionCodec.setBase(pos);
            }
            positionCodec.setBase(originalBase);
            return new Stepped(output);
        }

        public static record DeltaStep(short xa, short ya, short za, int ticks) {}
    }
}
