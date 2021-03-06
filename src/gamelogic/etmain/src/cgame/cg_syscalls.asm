code

equ	trap_PumpEventLoop                      -1
equ	trap_Print                              -2
equ	trap_Error                              -3
equ	trap_Milliseconds                       -4
equ	trap_Cvar_Register                      -5
equ	trap_Cvar_Update                        -6
equ	trap_Cvar_Set                           -7
equ	trap_Cvar_VariableStringBuffer          -8
equ	trap_Cvar_LatchedVariableStringBuffer   -9
equ	trap_Argc                               -10
equ	trap_Argv                               -11
equ	trap_Args                               -12
equ	trap_FS_FOpenFile                       -13
equ	trap_FS_Read                            -14
equ	trap_FS_Write                           -15
equ	trap_FS_FCloseFile                      -16
equ	trap_FS_GetFileList                     -17
equ	trap_FS_Delete                          -18
equ	trap_SendConsoleCommand                 -19
equ	trap_AddCommand                         -20
equ	trap_SendClientCommand                  -21
equ	trap_UpdateScreen                       -22
equ	trap_CM_NumInlineModels                 -23
equ	trap_CM_InlineModel                     -24
equ	trap_CM_TempBoxModel                    -25
equ	trap_CM_TempCapsuleModel                -26
equ	trap_CM_PointContents                   -27
equ	trap_CM_TransformedPointContents        -28
equ	trap_CM_BoxTrace                        -29
equ	trap_CM_TransformedBoxTrace             -30
equ	trap_CM_CapsuleTrace                    -31
equ	trap_CM_TransformedCapsuleTrace         -32
equ	trap_CM_MarkFragments                   -33
equ	trap_R_ProjectDecal                     -34
equ	trap_R_ClearDecals                      -35
equ	trap_S_StartSound                       -36
equ	trap_S_StartSoundVControl               -37
equ	trap_S_StartSoundEx                     -38
equ	trap_S_StartSoundExVControl             -39
equ	trap_S_StartLocalSound                  -40
equ	trap_S_ClearLoopingSounds               -41
equ	trap_S_ClearSounds                      -42
equ	trap_S_AddLoopingSound                  -43
equ	trap_S_AddRealLoopingSound              -44
equ	trap_S_StopStreamingSound               -45
equ	trap_S_UpdateEntityPosition             -46
equ	trap_S_GetVoiceAmplitude                -47
equ	trap_S_Respatialize                     -48
equ	trap_S_GetSoundLength                   -49
equ	trap_S_GetCurrentSoundTime              -50
equ	trap_S_StartBackgroundTrack             -51
equ	trap_S_FadeBackgroundTrack              -52
equ	trap_S_FadeAllSound                     -53
equ	trap_S_StartStreamingSound              -54
equ	trap_R_GetSkinModel                     -55
equ	trap_R_GetShaderFromModel               -56
equ	trap_R_ClearScene                       -57
equ	trap_R_AddRefEntityToScene              -58
equ	trap_R_AddRefLightToScene               -59
equ	trap_R_RegisterShaderLightAttenuation   -60
equ	trap_R_AddPolyToScene                   -61
equ	trap_R_AddPolyBufferToScene             -62
equ	trap_R_AddPolysToScene                  -63
equ	trap_R_AddLightToScene                  -64
equ	trap_R_AddCoronaToScene                 -65
equ	trap_R_SetFog                           -66
equ	trap_R_SetGlobalFog                     -67
equ	trap_R_RenderScene                      -68
equ	trap_R_SaveViewParms                    -69
equ	trap_R_RestoreViewParms                 -70
equ	trap_R_SetColor                         -71
equ	trap_R_DrawStretchPic                   -72
equ	trap_R_DrawRotatedPic                   -73
equ	trap_R_DrawStretchPicGradient           -74
equ	trap_R_Add2dPolys                       -75
equ	trap_R_ModelBounds                      -76
equ	trap_R_LerpTag                          -77
equ	trap_R_RemapShader                      -78
equ	trap_GetGlconfig                        -79
equ	trap_GetGameState                       -80
equ	trap_GetCurrentSnapshotNumber           -81
equ	trap_GetSnapshot                        -82
equ	trap_GetServerCommand                   -83
equ	trap_GetCurrentCmdNumber                -84
equ	trap_GetUserCmd                         -85
equ	trap_SetUserCmdValue                    -86
equ	trap_SetClientLerpOrigin                -87
equ	testPrintInt                            -88
equ	testPrintFloat                          -89
equ	trap_MemoryRemaining                    -90
equ	trap_loadCamera                         -91
equ	trap_startCamera                        -92
equ	trap_stopCamera                         -93
equ	trap_getCameraInfo                      -94
equ	trap_Key_IsDown                         -95
equ	trap_Key_GetCatcher                     -96
equ	trap_Key_GetOverstrikeMode              -97
equ	trap_Key_SetOverstrikeMode              -98
equ	trap_Key_KeysForBinding                 -99
equ	trap_Key_SetCatcher                     -100
equ	trap_Key_GetKey                         -101
equ	trap_PC_AddGlobalDefine                 -102
equ	trap_PC_LoadSource                      -103
equ	trap_PC_FreeSource                      -104
equ	trap_PC_ReadToken                       -105
equ	trap_PC_SourceFileAndLine               -106
equ	trap_PC_UnReadToken                     -107
equ	trap_S_StopBackgroundTrack              -108
equ	trap_RealTime                           -109
equ	trap_SnapVector                         -110
equ	trap_CIN_PlayCinematic                  -111
equ	trap_CIN_StopCinematic                  -112
equ	trap_CIN_RunCinematic                   -113
equ	trap_CIN_DrawCinematic                  -114
equ	trap_CIN_SetExtents                     -115
equ	trap_GetEntityToken                     -116
equ	trap_UI_Popup                           -117
equ	trap_UI_ClosePopup                      -118
equ	trap_Key_GetBindingBuf                  -119
equ	trap_Key_SetBinding                     -120
equ	trap_Key_KeynumToStringBuf              -121
equ	trap_TranslateString                    -122
equ	trap_S_RegisterSound                    -123
equ	trap_R_RegisterModel                    -124
equ	trap_R_RegisterSkin                     -125
equ	trap_R_RegisterShader                   -126
equ	trap_R_RegisterShaderNoMip              -127
equ	trap_R_RegisterFont                     -128
equ	trap_CM_LoadMap                         -129
equ	trap_R_LoadWorldMap                     -130
equ	trap_R_inPVS                            -131
equ	trap_GetHunkData                        -132
equ	trap_SendMessage                        -133
equ	trap_MessageStatus                      -134
equ	trap_R_LoadDynamicShader                -135
equ	trap_R_RenderToTexture                  -136
equ	trap_R_GetTextureId                     -137
equ	trap_R_Finish                           -138

equ	memset						            -201
equ	memcpy						            -202
equ	strncpy						            -203
equ	sin							            -204
equ	cos							            -205
equ	atan2						            -206
equ	sqrt						            -207
equ floor						            -208
equ	ceil						            -209
equ	testPrintInt				            -210
equ	testPrintFloat				            -211
equ acos						            -212