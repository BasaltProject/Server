// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;

import com.mojang.authlib.GameProfile;
import net.minecraft.network.chat.Component;
public interface ScoreHolder {
    String WILDCARD_NAME = "*";
    ScoreHolder WILDCARD = forNameOnly("*");
    default String getScoreboardName() {
        return "";
    }
    default Component getDisplayName() {
        return Component.literal(getScoreboardName());
    }
    default Component getFeedbackDisplayName() {
        return getDisplayName();
    }
    static ScoreHolder forNameOnly(String name) {
        return new ScoreHolder() {
            @Override
            public String getScoreboardName() {
                return name;
            }
        };
    }
    static ScoreHolder fromGameProfile(GameProfile profile) {
        return forNameOnly(profile.name());
    }
}
