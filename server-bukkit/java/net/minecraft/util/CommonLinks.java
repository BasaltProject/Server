// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.net.URI;
import java.util.UUID;

public class CommonLinks {
    public static final URI GDPR = URI.create("https://aka.ms/MinecraftGDPR");
    public static final URI EULA = URI.create("https://aka.ms/MinecraftEULA");
    public static final URI PRIVACY_STATEMENT = URI.create("https://aka.ms/MinecraftPrivacy");
    public static final URI ATTRIBUTION = URI.create("https://aka.ms/MinecraftJavaAttribution");
    public static final URI LICENSES = URI.create("https://aka.ms/MinecraftJavaLicenses");
    public static final URI BUY_MINECRAFT_JAVA = URI.create("https://aka.ms/BuyJavaMinecraft");
    public static final URI ACCOUNT_SETTINGS = URI.create("https://aka.ms/JavaAccountSettings");
    public static final URI SNAPSHOT_FEEDBACK = URI.create("https://aka.ms/snapshotfeedback?ref=game");
    public static final URI RELEASE_FEEDBACK = URI.create("https://aka.ms/javafeedback?ref=game");
    public static final URI SNAPSHOT_BUGS_FEEDBACK = URI.create("https://aka.ms/snapshotbugs?ref=game");
    public static final URI GENERAL_HELP = URI.create("https://aka.ms/Minecraft-Support");
    public static final URI ACCESSIBILITY_HELP = URI.create("https://aka.ms/MinecraftJavaAccessibility");
    public static final URI REPORTING_HELP = URI.create("https://aka.ms/aboutjavareporting");
    public static final URI SUSPENSION_HELP = URI.create("https://aka.ms/mcjavamoderation");
    public static final URI BLOCKING_HELP = URI.create("https://aka.ms/javablocking");
    public static final URI SYMLINK_HELP = URI.create("https://aka.ms/MinecraftSymLinks");
    public static final URI PRIVACY_AND_ONLINE_SETTINGS = URI.create("https://aka.ms/privacy-and-online-settings");
    public static final URI START_REALMS_TRIAL = URI.create("https://aka.ms/startjavarealmstrial");
    public static final URI BUY_REALMS = URI.create("https://aka.ms/BuyJavaRealms");
    public static final URI REALMS_TERMS = URI.create("https://aka.ms/MinecraftRealmsTerms");
    public static final URI REALMS_CONTENT_CREATION = URI.create("https://aka.ms/MinecraftRealmsContentCreator");
    public static final URI EXTEND_REALMS_LINK = URI.create("https://aka.ms/ExtendJavaRealms");
    public static final String INTENTIONAL_GAME_DESIGN_BUG_ID = "MCPE-28723";
    public static final URI INTENTIONAL_GAME_DESIGN_BUG = URI.create("https://bugs.mojang.com/browse/MCPE-28723");

    public static URI extendRealms(String subscriptionId, UUID profileId, ExtensionReference reference) {
        return URI.create(EXTEND_REALMS_LINK + "?subscriptionId=" + subscriptionId + "&profileId=" + profileId + "&ref=" + reference.name().toLowerCase(java.util.Locale.ROOT));
    }

    public static enum ExtensionReference {
        NONE, EXPIRED_TRIAL, EXPIRED_REALM;
    }
}
