// Copyright (C) 2020-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <seal/seal.h>

#include <memory>
#include <vector>

namespace intel {
namespace he {
namespace heseal {

// Stores all the components required for computation using BFV
class SealBFVContext {
 public:
  SealBFVContext(size_t poly_modulus_degree, std::vector<int> coeff_modulus,
                 bool generate_relin_keys = true,
                 bool generate_galois_keys = true)
      : m_poly_modulus_degree(poly_modulus_degree) {
    // SEAL uses an additional 'special prime' coeff modulus for relinearization
    // only. As such, encrypting with N coeff moduli yields a ciphertext with
    // N-1 coeff moduli for computation. See section 2.2.1 in
    // https://arxiv.org/pdf/1908.04172.pdf. So, we add an extra prime for fair
    // comparison against other HE schemes.
    coeff_modulus.push_back(60);

    m_parms.set_poly_modulus_degree(poly_modulus_degree);

    m_parms.set_coeff_modulus(
        seal::CoeffModulus::BFVDefault(poly_modulus_degree));

    m_parms.set_plain_modulus(
        seal::PlainModulus::Batching(poly_modulus_degree, 20));

    m_seal_context.reset(
        new seal::SEALContext(m_parms, true, seal::sec_level_type::none));
    m_keygen = std::make_unique<seal::KeyGenerator>(context());

    m_keygen->create_public_key(m_public_key);
    m_secret_key = m_keygen->secret_key();

    if (generate_relin_keys) {
      m_keygen->create_relin_keys(m_relin_keys);
    }
    if (generate_galois_keys) {
      m_keygen->create_galois_keys(m_galois_keys);
    }

    m_encryptor = std::make_unique<seal::Encryptor>(context(), m_public_key);
    m_evaluator = std::make_unique<seal::Evaluator>(context());
    m_decryptor = std::make_unique<seal::Decryptor>(context(), m_secret_key);
    m_batch_encoder = std::make_unique<seal::BatchEncoder>(context());
  }

  seal::BatchEncoder& batch_encoder() { return *m_batch_encoder; }
  seal::Evaluator& evaluator() { return *m_evaluator; }
  seal::Decryptor& decryptor() { return *m_decryptor; }
  const seal::Encryptor& encryptor() const { return *m_encryptor; }
  const seal::EncryptionParameters& parms() const { return m_parms; }
  const seal::PublicKey& public_key() const { return m_public_key; }
  const seal::SecretKey& secret_key() const { return m_secret_key; }
  const seal::RelinKeys& relin_keys() const { return m_relin_keys; }
  const seal::GaloisKeys& galois_keys() const { return m_galois_keys; }

  seal::SEALContext& context() { return *m_seal_context; }

  const int& poly_modulus_degree() const { return m_poly_modulus_degree; }

  // Encodes elements in v in-order to plaintexts.
  // Each Plaintext will encode batch-size elements, with possibly the exception
  // of the last plaintext.
  // For instance, if the batch_size is 3
  // [1, 2, 3, 4, 5, 6] => [Plain(1, 2, 3), Plain(4, 5, 6)].
  std::vector<seal::Plaintext> encodeVector(const gsl::span<const uint64_t>& v,
                                            size_t batch_size);

  // Decodes plaintext in v merges the resulting floating-point values in
  // order E.g. [Plain(1,2,3), Plain(4,5,6)] => [1,2,3,4,5,6] Retains the
  // first batch_size elements from each plaintext
  std::vector<uint64_t> decodeVector(const std::vector<seal::Plaintext>& v,
                                     size_t batch_size);

  // Encrypts each plaintext into a ciphertext using encryptor
  std::vector<seal::Ciphertext> encryptVector(
      const std::vector<seal::Plaintext>& plain);

  // Encodes elements in v in-order to plaintexts, and then encrypts them.
  // Each Plaintext will encode batch-size elements, with possibly the exception
  // of the last plaintext.
  // For instance, if the batch_size is 3
  // [1, 2, 3, 4, 5, 6] => [Plain(1, 2, 3), Plain(4, 5, 6)].
  inline std::vector<seal::Ciphertext> encryptVector(
      const gsl::span<const uint64_t>& v, size_t batch_size) {
    return encryptVector(encodeVector(v, batch_size));
  }

  // Decrypts each ciphertext into a plaintext using decryptor
  std::vector<seal::Plaintext> decryptVector(
      const std::vector<seal::Ciphertext>& cipher);

 private:
  int m_poly_modulus_degree;
  seal::EncryptionParameters m_parms{seal::scheme_type::bfv};
  std::shared_ptr<seal::SEALContext> m_seal_context;
  std::unique_ptr<seal::KeyGenerator> m_keygen;
  seal::PublicKey m_public_key;
  seal::SecretKey m_secret_key;
  seal::RelinKeys m_relin_keys;
  seal::GaloisKeys m_galois_keys;
  std::unique_ptr<seal::Encryptor> m_encryptor;
  std::unique_ptr<seal::Evaluator> m_evaluator;
  std::unique_ptr<seal::Decryptor> m_decryptor;
  std::unique_ptr<seal::BatchEncoder> m_batch_encoder;
};

}  // namespace heseal
}  // namespace he
}  // namespace intel
