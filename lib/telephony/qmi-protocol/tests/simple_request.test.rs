pub mod TEST {
    use crate::{Decodable, Encodable};
    use crate::QmiError;
    use bytes::{Bytes, Buf, BufMut, BytesMut};
    const MSG_MAX: usize = 512;
    pub struct TestReq {
        pub blah: u16,
    }
    impl TestReq {
        pub fn new(blah: u16,) -> Self {
            TestReq {
                blah,
            }
        }
    }
    #[derive(Debug)]
    pub struct TestResp {
    }
    impl Encodable for TestReq {
        fn transaction_id_len(&self) -> u8 {
            2
        }
        fn svc_id(&self) -> u8 {
            66
        }
        fn to_bytes(&self) -> (Bytes, u16) {
            let mut buf = BytesMut::with_capacity(MSG_MAX);
            // svc id
            buf.put_u16_le(288);
            // svc length calculation
            let mut Test_len = 0u16;
            Test_len += 1; // tlv type length;
            Test_len += 2; // tlv length length;
            let blah = &self.blah;
            Test_len += 2;
            buf.put_u16_le(Test_len);
            // tlvs
            let blah = &self.blah;
            buf.put_u8(1);
            buf.put_u16_le(2);
            buf.put_u16_le(*blah);
            return (buf.freeze(), Test_len + 2 /*msg id len*/ + 2 /*msg len len*/);
        }
    }
    impl Decodable for TestResp {
        fn from_bytes<T: Buf>(mut buf: T) -> Result<TestResp, QmiError> {
            assert_eq!(288, buf.get_u16_le());
            let mut total_len = buf.get_u16_le();
            let _ = buf.get_u8();
            total_len -= 1;
            let res_len = buf.get_u16_le();
            total_len -= (res_len + 2);
            if 0x00 != buf.get_u16_le() {
                let error_code = buf.get_u16_le();
                return Err(QmiError::from_code(error_code))
            } else {
                assert_eq!(0x00, buf.get_u16_le()); // this must be zero if no error from above check
            }
            while total_len > 0 {
                let msg_id = buf.get_u8();
                total_len -= 1;
                let tlv_len = buf.get_u16_le();
                total_len -= 2;
                match msg_id {
                    _ => panic!("unknown id for this message type")
                }
            }
            Ok(TestResp {
            })
        }
    }
}
