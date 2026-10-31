// Defaults for legacy monster definitions. Authored monster_seek_range traits take priority.
class UDWOMonsterSettings : UDataAsset
{
 UPROPERTY(EditAnywhere,Category="Perception",Meta=(ClampMin="0",Units="m",ToolTip="Fallback horizontal range for monsters without a monster_seek_range trait."))
 double AggroRadiusMeters=20.0;
 UPROPERTY(EditAnywhere,Category="Spawning",Meta=(ClampMin="0",Units="m",ToolTip="Fixed horizontal no-spawn radius around the player's floor start."))
 double PlayerSpawnExclusionRadiusMeters=10.0;
}
