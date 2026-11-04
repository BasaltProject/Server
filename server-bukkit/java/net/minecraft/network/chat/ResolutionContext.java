// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;
import java.util.function.Predicate;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.contents.objects.ObjectInfo;
import net.minecraft.world.entity.Entity;
import org.apache.commons.lang3.mutable.MutableInt;
public record ResolutionContext(CommandSourceStack source, Entity defaultScoreboardEntity, Predicate<ObjectInfo> objectInfoValidator, int depthLimit, LimitBehavior depthLimitBehavior, int resolvedComponentLimit, MutableInt resolvedComponentCount) {
    private static final int kDefaultResolvedComponentLimit = 65536;
    public ObjectInfo validate(ObjectInfo description) {
        return objectInfoValidator.test(description) ? description : null;
    }
    public static ResolutionContext create(CommandSourceStack source) {
        return builder().withSource(source).build();
    }
    public static Builder builder() {
        return new Builder();
    }
    public static enum LimitBehavior {
        DISCARD_REMAINING, STOP_PROCESSING_AND_COPY_REMAINING;
    }
    public static class Builder {
        private CommandSourceStack source;
        private Entity defaultScoreboardEntity;
        private Predicate<ObjectInfo> objectInfoValidator = info -> true;
        private int depthLimit = 100;
        private LimitBehavior depthLimitBehavior = LimitBehavior.STOP_PROCESSING_AND_COPY_REMAINING;
        public Builder() {
        }
        public Builder withSource(CommandSourceStack source) {
            this.source = source;
            this.defaultScoreboardEntity = source.getEntity();
            return this;
        }
        public Builder withEntityOverride(Entity defaultScoreboardEntity) {
            this.defaultScoreboardEntity = defaultScoreboardEntity;
            return this;
        }
        public Builder withObjectInfoValidator(Predicate<ObjectInfo> objectInfoValidator) {
            this.objectInfoValidator = objectInfoValidator;
            return this;
        }
        public Builder setDepthLimit(int depthLimit) {
            this.depthLimit = depthLimit;
            return this;
        }
        public Builder setDepthLimitBehavior(LimitBehavior behavior) {
            this.depthLimitBehavior = behavior;
            return this;
        }
        public ResolutionContext build() {
            return new ResolutionContext(source, defaultScoreboardEntity, objectInfoValidator, depthLimit, depthLimitBehavior, kDefaultResolvedComponentLimit, new MutableInt());
        }
    }
}
