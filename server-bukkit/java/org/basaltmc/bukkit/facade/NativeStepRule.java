package org.basaltmc.bukkit.facade;

import com.mojang.datafixers.RewriteResult;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.types.Type;
import java.util.Optional;

/** The rule of a vanilla fix, which the native fixer runs by name; the Java rewrite engine never applies it. */
public final class NativeStepRule implements TypeRewriteRule {
    private final String step;
    private final int versionKey;

    private NativeStepRule(String step, int versionKey) {
        this.step = step;
        this.versionKey = versionKey;
    }

    public static TypeRewriteRule of(String step, int versionKey) {
        return new NativeStepRule(step, versionKey);
    }

    public String step() {
        return step;
    }

    public int versionKey() {
        return versionKey;
    }

    @Override
    public <A> Optional<RewriteResult<A, ?>> rewrite(Type<A> type) {
        throw new UnsupportedOperationException(step + " runs in the native data fixer; use DataFixTypes");
    }
}
