
TEST_F(DISABLED_MMW_RX_CAL_TEST, ftm_mmw_cal_unpack_bld_exec_rx_rf_relative_gain_cal)
{
  ftm_calv3_mmw_rx_seq_class* p_mmw_rx_cal;
  uint8 payload[1000];
  uint16 type_id;
  uint16 type_sz;

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "**********MMW RX CAL : Build RX RF Relative Gain Cal Test: BEGIN *********");

  radio_setup(-1, 0, 2); // Rx only, RF loop back

  /* Build packet 1*/
  payload[0] = 0; /* Num_params_types*/
  uint8* payload_ptr = &payload[1];

  /*-------------- Rx Cal Type ----------------*/
  /* TYPE ID pack */
  type_id = MMW_RX_CAL_TYPE_PARAM;
  memscpy(payload_ptr,  sizeof(uint16), &type_id, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE SZ pack */
  type_sz = 2;
  memscpy(payload_ptr,  sizeof(uint16), &type_sz, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  uint16 temp16 = MMW_RX_RF_RELATIVE_GAIN_CAL;
  memscpy(payload_ptr, sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  payload[0]++; /* Num_params_types*/

  /*-------------- Path Params ----------------*/
  /* TYPE ID pack */
  type_id = MMW_RX_CAL_PATH_PARAMS;
  memscpy(payload_ptr,  sizeof(uint16), &type_id, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE SZ pack */
  type_sz = 5; //uint32 + uint8
  memscpy(payload_ptr,  sizeof(uint16), &type_sz, sizeof(uint16));
  payload_ptr += sizeof(uint16);


  /* TYPE payload */
  uint32 temp32 = 3; // RF Trx device bit mask (bits 0 and 1 are set)
  memscpy(payload_ptr, sizeof(uint32), &temp32, sizeof(uint32));
  payload_ptr += sizeof(uint32);

  /* TYPE payload */
  uint8 temp8 = 0; // Current antenna group 0
  memscpy(payload_ptr, sizeof(uint8), &temp8, sizeof(uint8));
  payload_ptr += sizeof(uint8);

  payload[0]++; /* Num_params_types*/

  /*-------------- Step Duration ----------------*/
  /* TYPE ID pack */
  type_id = MMW_RX_CAL_STEP_DURATION_PARAM;
  memscpy(payload_ptr,  sizeof(uint16), &type_id, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE SZ pack */
  type_sz = 2;
  memscpy(payload_ptr,  sizeof(uint16), &type_sz, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp16 = 200; // Step duration = 200us
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  payload[0]++; /* Num_params_types*/

  /*-------------- Frequency params ----------------*/
  /* TYPE ID pack */
  type_id = MMW_RX_CAL_FREQUENCY_PARAMS;
  memscpy(payload_ptr,  sizeof(uint16), &type_id, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE SZ pack */
  type_sz = 6; // uint16 + uint32
  memscpy(payload_ptr,  sizeof(uint16), &type_sz, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp16 = 1; // num frequencys = 1
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp32 = 28000000; // frequency in kHz = 28,000,000
  memscpy(payload_ptr,  sizeof(uint32), &temp32, sizeof(uint32));
  payload_ptr += sizeof(uint32);

  payload[0]++; /* Num_params_types*/

  /*-------------- rel Gain cal params ----------------*/
  /* TYPE ID pack */
  type_id = MMW_RX_REL_GAIN_CAL_PARAMS;
  memscpy(payload_ptr,  sizeof(uint16), &type_id, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE SZ pack */
  type_sz = 12; //uint8 + uint8 * 3 + uint16 * 3 + uint16
  memscpy(payload_ptr,  sizeof(uint16), &type_sz, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp8 = 3; // Num gain states = 3
  memscpy(payload_ptr,  sizeof(uint8), &temp8, sizeof(uint8));
  payload_ptr += sizeof(uint8);

  /* TYPE payload */
  temp8 = 0; // gain state[0]
  memscpy(payload_ptr,  sizeof(uint8), &temp8, sizeof(uint8));
  payload_ptr += sizeof(uint8);

  /* TYPE payload */
  temp8 = 1; // gain state[1]
  memscpy(payload_ptr,  sizeof(uint8), &temp8, sizeof(uint8));
  payload_ptr += sizeof(uint8);

  /* TYPE payload */
  temp8 = 2; // gain state[2]
  memscpy(payload_ptr,  sizeof(uint8), &temp8, sizeof(uint8));
  payload_ptr += sizeof(uint8);

  /* TYPE payload */
  temp16 = 10; // RGI[0]
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp16 = 20; // RGI[1]
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp16 = 30; // RGI[2]
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  /* TYPE payload */
  temp16 = 100; // AGC Measurement Duration = 100us
  memscpy(payload_ptr,  sizeof(uint16), &temp16, sizeof(uint16));
  payload_ptr += sizeof(uint16);

  payload[0]++; /* Num_params_types*/

  // Initialize
  p_mmw_rx_cal = new ftm_calv3_mmw_rx_seq_class(4534, /* handle */
                                                SEQ_MMW_RX_CAL); /* sequence_type */

  p_mmw_rx_cal->m_enableDebugMode = TRUE;

  // Unpack
  p_mmw_rx_cal->unpack_build_params(&payload[0]);
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_BUILD_RECEIVED);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  // Build
  p_mmw_rx_cal->process_build();
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_BUILD_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  // unpack exec
  p_mmw_rx_cal->unpack_exec_params(NULL);
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_RECEIVED);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  // Exec
  p_mmw_rx_cal->process_exec();
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  delete p_mmw_rx_cal;

  radio_tear_down();

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "**********MMW RX CAL : Build RX RF Relative Gain Cal Test: DONE**********");
}

TEST_F(DISABLED_MMW_RX_CAL_TEST, ftm_mmw_cal_rx_rf_gain_cal)
{
  ftm_calv3_mmw_rx_seq_class* p_mmw_rx_cal;
  ftm_cal_seq_mmw_rx_bld_params_t bld_params;
  radio_config_rfm_path_params context;

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "**********MMW RX CAL : RX RF Gain Cal Test: BEGIN**********");

  /* Temporarily changed this radio setup to Rx Only, RF Loop Back.
  Return to no loop back and uncomment radio setup in Line 1194 later */
  //radio_setup(-1, 0, 0); // Rx Only, No Loopback
  radio_setup(1, 0, 0); // Rx + Tx, No Loop back - HACK FOR NOW

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "_____________Absolute Gain Cal_____________");

  memset(&bld_params, 0, sizeof(ftm_cal_seq_mmw_rx_bld_params_t));

  /* Set Radio setup context */
  context.rfm_path.rfm_device = RFM_DEVICE_0; /* RX device */
  context.band = 0; /* RFI_NR5G_BAND_1_SUBBAND_1*/

  bld_params.curr_ant_grp = 0;

  /* Set params to do rx rf absolute gain cal */
  bld_params.cal_purpose = MMW_RX_RF_ABSOLUTE_GAIN_CAL;
  bld_params.per_actn_step_dur = 200; //200us
  bld_params.rx_rf_abs_gain_params.num_gain_states = 1;
  bld_params.rx_rf_abs_gain_params.gain_states[0] = 0;
  bld_params.rx_rf_droop_freq_params.num_droop_freqs = 1;
  bld_params.rx_rf_droop_freq_params.droop_frequency[0] = 0;
  bld_params.rx_rf_abs_gain_params.agc_meas_duration_us = 100; //100us

  bld_params.num_rf_trx_dev_to_cal = 2;
  bld_params.rf_trx_id_list[0] = 0;
  bld_params.rf_trx_id_list[1] = 1;

  p_mmw_rx_cal = new ftm_calv3_mmw_rx_seq_class(4534, /* handle */
                                                SEQ_MMW_RX_CAL, /* sequence_type */
                                                &bld_params,
                                                FTM_RF_TECH_NR5G, /* ftm_rf_technology_type */
                                                &context);

  p_mmw_rx_cal->m_enableDebugMode = TRUE;

  p_mmw_rx_cal->process_build();

  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_BUILD_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  p_mmw_rx_cal->unpack_exec_params(NULL);
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_RECEIVED);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  p_mmw_rx_cal->process_exec();
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  radio_setup(-1, 0, 2); // Rx only, RF loop back

  /* Set params to do rx rf relative gain cal */
  bld_params.cal_purpose = MMW_RX_RF_RELATIVE_GAIN_CAL;
  bld_params.per_actn_step_dur = 200; //200us
  bld_params.rx_rf_rel_gain_params.num_gain_states = 3;
  bld_params.rx_rf_rel_gain_params.gain_states[0] = 0;
  bld_params.rx_rf_rel_gain_params.gain_states[1] = 1;
  bld_params.rx_rf_rel_gain_params.gain_states[2] = 2;
  bld_params.rx_rf_rel_gain_params.rgi_list[0] = 10;
  bld_params.rx_rf_rel_gain_params.rgi_list[1] = 20;
  bld_params.rx_rf_rel_gain_params.rgi_list[2] = 30;
  bld_params.rx_rf_droop_freq_params.num_droop_freqs = 1;
  bld_params.rx_rf_droop_freq_params.droop_frequency[0] = 0;
  bld_params.rx_rf_rel_gain_params.agc_meas_duration_us = 100; //100us

  bld_params.num_rf_trx_dev_to_cal = 2;
  bld_params.rf_trx_id_list[0] = 0;
  bld_params.rf_trx_id_list[1] = 1;

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "_____________Relative Gain Cal_____________");

  p_mmw_rx_cal->ftm_calv3_mmw_rx_seq_class_update_params(&bld_params);

  p_mmw_rx_cal->process_build();

  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_BUILD_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  p_mmw_rx_cal->unpack_exec_params(NULL);
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_RECEIVED);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  p_mmw_rx_cal->process_exec();
  ASSERT(p_mmw_rx_cal->m_curr_seq_state == CAL_SEQ_EXEC_DONE);
  ASSERT(p_mmw_rx_cal->m_res_code == MMW_RX_CAL_SEQ_PASS);

  delete p_mmw_rx_cal;

  radio_tear_down();

  RF_MESSAGE(RFCMN_MSG_H, RF_CMN_FTM, RF_HI, "**********MMW RX CAL : RX RF Gain Cal Test: DONE**********");
}
