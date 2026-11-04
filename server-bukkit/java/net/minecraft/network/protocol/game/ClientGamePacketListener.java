// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.ConnectionProtocol;
import net.minecraft.network.protocol.common.ClientCommonPacketListener;
import net.minecraft.network.protocol.ping.ClientPongPacketListener;
public interface ClientGamePacketListener extends ClientCommonPacketListener, ClientPongPacketListener {    default ConnectionProtocol protocol() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#protocol");
    }
    default void handleAddEntity(ClientboundAddEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleAddEntity");
    }
    default void handleAddObjective(ClientboundSetObjectivePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleAddObjective");
    }
    default void handleAnimate(ClientboundAnimatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleAnimate");
    }
    default void handleHurtAnimation(ClientboundHurtAnimationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleHurtAnimation");
    }
    default void handleSwingAnimation(ClientboundSwingAnimationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSwingAnimation");
    }
    default void handleAwardStats(ClientboundAwardStatsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleAwardStats");
    }
    default void handleRecipeBookAdd(ClientboundRecipeBookAddPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRecipeBookAdd");
    }
    default void handleRecipeBookRemove(ClientboundRecipeBookRemovePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRecipeBookRemove");
    }
    default void handleRecipeBookSettings(ClientboundRecipeBookSettingsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRecipeBookSettings");
    }
    default void handleBlockDestruction(ClientboundBlockDestructionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBlockDestruction");
    }
    default void handleOpenSignEditor(ClientboundOpenSignEditorPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleOpenSignEditor");
    }
    default void handleBlockEntityData(ClientboundBlockEntityDataPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBlockEntityData");
    }
    default void handleBlockEvent(ClientboundBlockEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBlockEvent");
    }
    default void handleBlockUpdate(ClientboundBlockUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBlockUpdate");
    }
    default void handleSystemChat(ClientboundSystemChatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSystemChat");
    }
    default void handlePlayerChat(ClientboundPlayerChatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerChat");
    }
    default void handleDisguisedChat(ClientboundDisguisedChatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDisguisedChat");
    }
    default void handleDeleteChat(ClientboundDeleteChatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDeleteChat");
    }
    default void handleChunkBlocksUpdate(ClientboundSectionBlocksUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleChunkBlocksUpdate");
    }
    default void handleMapItemData(ClientboundMapItemDataPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMapItemData");
    }
    default void handleContainerClose(ClientboundContainerClosePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleContainerClose");
    }
    default void handleContainerContent(ClientboundContainerSetContentPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleContainerContent");
    }
    default void handleMountScreenOpen(ClientboundMountScreenOpenPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMountScreenOpen");
    }
    default void handleContainerSetData(ClientboundContainerSetDataPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleContainerSetData");
    }
    default void handleContainerSetSlot(ClientboundContainerSetSlotPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleContainerSetSlot");
    }
    default void handleEntityEvent(ClientboundEntityEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleEntityEvent");
    }
    default void handleEntityLinkPacket(ClientboundSetEntityLinkPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleEntityLinkPacket");
    }
    default void handleSetEntityPassengersPacket(ClientboundSetPassengersPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetEntityPassengersPacket");
    }
    default void handleAddTransientBlockPacket(ClientboundAddTransientBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleAddTransientBlockPacket");
    }
    default void handleExplosion(ClientboundExplodePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleExplosion");
    }
    default void handleGameEvent(ClientboundGameEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleGameEvent");
    }
    default void handleLevelChunkWithLight(ClientboundLevelChunkWithLightPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLevelChunkWithLight");
    }
    default void handleChunksBiomes(ClientboundChunksBiomesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleChunksBiomes");
    }
    default void handleForgetLevelChunk(ClientboundForgetLevelChunkPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleForgetLevelChunk");
    }
    default void handleLevelEvent(ClientboundLevelEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLevelEvent");
    }
    default void handleLogin(ClientboundLoginPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLogin");
    }
    default void handleMoveEntity(ClientboundMoveEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMoveEntity");
    }
    default void handleMinecartAlongTrack(ClientboundMoveMinecartPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMinecartAlongTrack");
    }
    default void handleMovePlayer(ClientboundPlayerPositionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMovePlayer");
    }
    default void handleRotatePlayer(ClientboundPlayerRotationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRotatePlayer");
    }
    default void handleParticleEvent(ClientboundLevelParticlesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleParticleEvent");
    }
    default void handlePlayerAbilities(ClientboundPlayerAbilitiesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerAbilities");
    }
    default void handleGameRuleValues(ClientboundGameRuleValuesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleGameRuleValues");
    }
    default void handlePlayerInfoRemove(ClientboundPlayerInfoRemovePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerInfoRemove");
    }
    default void handlePlayerInfoUpdate(ClientboundPlayerInfoUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerInfoUpdate");
    }
    default void handleRemoveEntities(ClientboundRemoveEntitiesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRemoveEntities");
    }
    default void handleRemoveMobEffect(ClientboundRemoveMobEffectPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRemoveMobEffect");
    }
    default void handleRespawn(ClientboundRespawnPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRespawn");
    }
    default void handleRotateMob(ClientboundRotateHeadPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleRotateMob");
    }
    default void handleSetHeldSlot(ClientboundSetHeldSlotPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetHeldSlot");
    }
    default void handleSetDisplayObjective(ClientboundSetDisplayObjectivePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetDisplayObjective");
    }
    default void handleSetEntityData(ClientboundSetEntityDataPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetEntityData");
    }
    default void handleSetEntityMotion(ClientboundSetEntityMotionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetEntityMotion");
    }
    default void handleSetEquipment(ClientboundSetEquipmentPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetEquipment");
    }
    default void handleSetExperience(ClientboundSetExperiencePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetExperience");
    }
    default void handleSetHealth(ClientboundSetHealthPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetHealth");
    }
    default void handleSetPlayerTeamPacket(ClientboundSetPlayerTeamPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetPlayerTeamPacket");
    }
    default void handleSetScore(ClientboundSetScorePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetScore");
    }
    default void handleResetScore(ClientboundResetScorePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleResetScore");
    }
    default void handleSetSpawn(ClientboundSetDefaultSpawnPositionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetSpawn");
    }
    default void handleSetTime(ClientboundSetTimePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetTime");
    }
    default void handleSoundEvent(ClientboundSoundPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSoundEvent");
    }
    default void handleSoundEntityEvent(ClientboundSoundEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSoundEntityEvent");
    }
    default void handleTakeItemEntity(ClientboundTakeItemEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTakeItemEntity");
    }
    default void handleEntityPositionSync(ClientboundEntityPositionSyncPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleEntityPositionSync");
    }
    default void handleTeleportEntity(ClientboundTeleportEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTeleportEntity");
    }
    default void handleTickingState(ClientboundTickingStatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTickingState");
    }
    default void handleTickingStep(ClientboundTickingStepPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTickingStep");
    }
    default void handleUpdateAttributes(ClientboundUpdateAttributesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleUpdateAttributes");
    }
    default void handleUpdateMobEffect(ClientboundUpdateMobEffectPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleUpdateMobEffect");
    }
    default void handlePlayerCombatEnd(ClientboundPlayerCombatEndPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerCombatEnd");
    }
    default void handlePlayerCombatEnter(ClientboundPlayerCombatEnterPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerCombatEnter");
    }
    default void handlePlayerCombatKill(ClientboundPlayerCombatKillPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlayerCombatKill");
    }
    default void handleChangeDifficulty(ClientboundChangeDifficultyPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleChangeDifficulty");
    }
    default void handleSetCamera(ClientboundSetCameraPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetCamera");
    }
    default void handleInitializeBorder(ClientboundInitializeBorderPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleInitializeBorder");
    }
    default void handleSetBorderLerpSize(ClientboundSetBorderLerpSizePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetBorderLerpSize");
    }
    default void handleSetBorderSize(ClientboundSetBorderSizePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetBorderSize");
    }
    default void handleSetBorderWarningDelay(ClientboundSetBorderWarningDelayPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetBorderWarningDelay");
    }
    default void handleSetBorderWarningDistance(ClientboundSetBorderWarningDistancePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetBorderWarningDistance");
    }
    default void handleSetBorderCenter(ClientboundSetBorderCenterPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetBorderCenter");
    }
    default void handleTabListCustomisation(ClientboundTabListPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTabListCustomisation");
    }
    default void handleBossUpdate(ClientboundBossEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBossUpdate");
    }
    default void handleItemCooldown(ClientboundCooldownPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleItemCooldown");
    }
    default void handleMoveVehicle(ClientboundMoveVehiclePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMoveVehicle");
    }
    default void handleUpdateAdvancementsPacket(ClientboundUpdateAdvancementsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleUpdateAdvancementsPacket");
    }
    default void handleSelectAdvancementsTab(ClientboundSelectAdvancementsTabPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSelectAdvancementsTab");
    }
    default void handlePlaceRecipe(ClientboundPlaceGhostRecipePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handlePlaceRecipe");
    }
    default void handleCommands(ClientboundCommandsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleCommands");
    }
    default void handleStopSoundEvent(ClientboundStopSoundPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleStopSoundEvent");
    }
    default void handleCommandSuggestions(ClientboundCommandSuggestionsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleCommandSuggestions");
    }
    default void handleUpdateRecipes(ClientboundUpdateRecipesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleUpdateRecipes");
    }
    default void handleLookAt(ClientboundPlayerLookAtPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLookAt");
    }
    default void handleTagQueryPacket(ClientboundTagQueryPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTagQueryPacket");
    }
    default void handleLightUpdatePacket(ClientboundLightUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLightUpdatePacket");
    }
    default void handleOpenBook(ClientboundOpenBookPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleOpenBook");
    }
    default void handleOpenScreen(ClientboundOpenScreenPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleOpenScreen");
    }
    default void handleMerchantOffers(ClientboundMerchantOffersPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleMerchantOffers");
    }
    default void handleSetChunkCacheRadius(ClientboundSetChunkCacheRadiusPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetChunkCacheRadius");
    }
    default void handleSetSimulationDistance(ClientboundSetSimulationDistancePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetSimulationDistance");
    }
    default void handleSetChunkCacheCenter(ClientboundSetChunkCacheCenterPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetChunkCacheCenter");
    }
    default void handleBlockChangedAck(ClientboundBlockChangedAckPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBlockChangedAck");
    }
    default void setActionBarText(ClientboundSetActionBarTextPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#setActionBarText");
    }
    default void setSubtitleText(ClientboundSetSubtitleTextPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#setSubtitleText");
    }
    default void setTitleText(ClientboundSetTitleTextPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#setTitleText");
    }
    default void setTitlesAnimation(ClientboundSetTitlesAnimationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#setTitlesAnimation");
    }
    default void handleTitlesClear(ClientboundClearTitlesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTitlesClear");
    }
    default void handleServerData(ClientboundServerDataPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleServerData");
    }
    default void handleCustomChatCompletions(ClientboundCustomChatCompletionsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleCustomChatCompletions");
    }
    default void handleBundlePacket(ClientboundBundlePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleBundlePacket");
    }
    default void handleDamageEvent(ClientboundDamageEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDamageEvent");
    }
    default void handleConfigurationStart(ClientboundStartConfigurationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleConfigurationStart");
    }
    default void handleChunkBatchStart(ClientboundChunkBatchStartPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleChunkBatchStart");
    }
    default void handleChunkBatchFinished(ClientboundChunkBatchFinishedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleChunkBatchFinished");
    }
    default void handleDebugSample(ClientboundDebugSamplePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDebugSample");
    }
    default void handleProjectilePowerPacket(ClientboundProjectilePowerPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleProjectilePowerPacket");
    }
    default void handleSetCursorItem(ClientboundSetCursorItemPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetCursorItem");
    }
    default void handleSetPlayerInventory(ClientboundSetPlayerInventoryPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleSetPlayerInventory");
    }
    default void handleTestInstanceBlockStatus(ClientboundTestInstanceBlockStatus packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleTestInstanceBlockStatus");
    }
    default void handleWaypoint(ClientboundTrackedWaypointPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleWaypoint");
    }
    default void handleDebugChunkValue(ClientboundDebugChunkValuePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDebugChunkValue");
    }
    default void handleDebugBlockValue(ClientboundDebugBlockValuePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDebugBlockValue");
    }
    default void handleDebugEntityValue(ClientboundDebugEntityValuePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDebugEntityValue");
    }
    default void handleDebugEvent(ClientboundDebugEventPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleDebugEvent");
    }
    default void handleGameTestHighlightPos(ClientboundGameTestHighlightPosPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleGameTestHighlightPos");
    }
    default void handleLowDiskSpaceWarning(ClientboundLowDiskSpaceWarningPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientGamePacketListener#handleLowDiskSpaceWarning");
    }
}
